#include "CompositeCommandsModule.h"
#include "ModuleController.h"
#include <EEPROM.h>
#include "InteropStream.h"

void CompositeCommandsModule::Setup()
{
  // настройка модуля тут
  LoadCommands();
}
void CompositeCommandsModule::Clear()
{
  size_t cnt = commands.size();
  for(size_t i=0;i<cnt;i++)
  {
      CompositeCommands* cc = commands[i];
      for(size_t j=0;j<cc->Commands.size();j++)
      {
        CompositeCommand* cCom = cc->Commands[j];
        delete cCom;
      }
      cc->Commands.Clear();

      delete cc;
  }
  
  commands.Clear();
}
void CompositeCommandsModule::LoadCommands()
{
  Clear();
  
  uint16_t addr = COMPOSITE_COMMANDS_START_ADDR;
  // читаем кол-во команд
  uint8_t cnt = EEPROM.read(addr++);
  
  if(cnt == 0xFF) // ничего не сохранено
    return;

  // последовательно читаем все команды
  for(uint8_t i=0;i<cnt;i++)
  {
    // для каждой команды читаем кол-во дочерних
      CompositeCommands* newCmds = new CompositeCommands;
      uint8_t childCount = EEPROM.read(addr++);

    // последовательно читаем дочерние команды
    for(uint8_t j=0;j<childCount;j++)
    {
      // для каждой команды читаем
      CompositeCommand* childCommand = new CompositeCommand;
      // тип действия
      childCommand->command = EEPROM.read(addr++);
      
      // дополнительные параметры
      childCommand->data = EEPROM.read(addr++);
      // и помещаем её в список команд для команды
      newCmds->Commands.push_back(childCommand);
      
      
    } // for
    
    // помещаем составную команду в общий список
    commands.push_back(newCmds);
  } // for
  
  // всё прочитано
}
void CompositeCommandsModule::SaveCommands()
{
  // сохраняем команды в EEPROM
    uint16_t addr = COMPOSITE_COMMANDS_START_ADDR;
    
    size_t cnt = commands.size();
  // сначала пишем кол-во команд
    EEPROM.write(addr++,(uint8_t)cnt);
    
    for(size_t i=0;i<cnt;i++)
    {
        // потом для каждой команды пишем
        CompositeCommands* cCommands = commands[i];
        // кол-во её дочерних команд
        size_t child_cnt = cCommands->Commands.size();
        EEPROM.write(addr++,(uint8_t)child_cnt);
        // и дочерние команды
        for(size_t j=0;j<child_cnt;j++)
        {
          // для каждой дочерней пишем
          CompositeCommand* child = cCommands->Commands[j];
          // действие
          EEPROM.write(addr++,child->command);
          // дополнительные параметры
          EEPROM.write(addr++,child->data);
        } // for
    } // for
    
    // записали
}
void CompositeCommandsModule::Update(uint16_t dt)
{ 
  UNUSED(dt);
  // обновление модуля тут

}
void CompositeCommandsModule::AddCommand(uint8_t listIdx, uint8_t action, uint8_t param)
{
  // проверяем, есть ли такой индекс в списке
  if(commands.size() <= listIdx)
  {
    // если нет - то добавляем пустой список
    CompositeCommands* cc = new CompositeCommands;
    commands.push_back(cc);
  }
  // потом добавляем в этот список команду на выполнение
  CompositeCommands* commandsList = commands[listIdx];
  CompositeCommand* cmd = new CompositeCommand; 
  cmd->command = action; 
  cmd->data = param;
  
  commandsList->Commands.push_back(cmd);

}
void CompositeCommandsModule::ProcessCommand(uint8_t idx)
{
  if(commands.size() <= idx) // чего-то с индексом не сложилось
    return;

  // получаем список команд на выполнение  
  CompositeCommands* commandsList = commands[idx];
  
  // проходимся по каждой команде, и из списка выполняем все перечисленные
  size_t cnt = commandsList->Commands.size();

  String textCommand; // текстовая команда на выполнение
  for(size_t i=0;i<cnt;i++)
  {
    yield(); // даём поработать другим модулям
    
    CompositeCommand* command = commandsList->Commands[i];

    // смотрим, что за команда
    switch(command->command)
    {
      case ccCloseWindows: // закрыть форточки
      {
        textCommand = F("STATE|WINDOW|ALL|CLOSE");
      }
      break;
      
      case ccOpenWindows: // открыть форточки
      {
        textCommand = F("STATE|WINDOW|ALL|OPEN");
      }
      break;
      
      case ccLightOff: // выключить досветку
      {
        textCommand = F("LIGHT|OFF");
      }
      break;
      
      case ccLightOn: // включить досветку
      {
        textCommand = F("LIGHT|ON");
      }
      break;
      
      case ccPinOff: // выставить на пине низкий уровень
      {
        textCommand = F("PIN|");
        textCommand += String(command->data);
        textCommand += PARAM_DELIMITER;
        textCommand += STATE_OFF;
      }
      break;
      
      case ccPinOn: // выставить на пине высокий уровень
      {
        textCommand = F("PIN|");
        textCommand += String(command->data);
        textCommand += PARAM_DELIMITER;
        textCommand += STATE_ON;
      }
      break;
      
    } // switch
    
    if(textCommand.length())
    {
      // выполняем команду.
      ModuleInterop.QueryCommand(ctSET,textCommand,true,false);
    }
  
  } // for
    
    // все команды для составной - выполнены
    
}
bool  CompositeCommandsModule::ExecCommand(const Command& command, bool wantAnswer)
{

  if(wantAnswer)
    PublishSingleton = UNKNOWN_COMMAND;
    
  size_t argsCount = command.GetArgsCount();
  
  
  if(command.GetType() == ctGET)
  {
    if(wantAnswer)
      PublishSingleton = NOT_SUPPORTED;
  }
  else
  if(command.GetType() == ctSET)
  {
    if(argsCount < 1)
    {
      PublishSingleton = PARAMS_MISSED;
    }
    else
    {
      // есть параметры
      String which = command.GetArg(0);
      
      if(which == CC_DELETE_COMMAND) // очистить все команды
      {
        Clear();
        if(wantAnswer)
          PublishSingleton = REG_DEL; // говорим, что удалили всё

        PublishSingleton.Status = true;
      } // CC_DELETE_COMMAND
      else
      if(which == CC_SAVE_COMMAND) // сохранить команды в EEPROM
      {
        SaveCommands();
        if(wantAnswer)
          PublishSingleton = REG_SUCC; // говорим, что сохранили

        PublishSingleton.Status = true;
      } // CC_SAVE_COMMAND
      else
      if(which == CC_PROCESS_COMMAND) // выполнить команду
      {
          if(argsCount > 1)
          {
            // хватает аргументов
            uint8_t cmd = abs(atoi(command.GetArg(1)));//String(command.GetArg(1)).toInt());
            ProcessCommand(cmd); // выполняем составную команду
            
            if(wantAnswer)
              PublishSingleton = REG_SUCC; // говорим, что выполнили

            PublishSingleton.Status = true;
          }
          else
          {
            // не хватает аргументов
            if(wantAnswer)
              PublishSingleton = PARAMS_MISSED;
          }
      } // CC_PROCESS_COMMAND
      else
      if(which == CC_ADD_COMMAND) // добавить параметр в список составной команды
      {
        if(argsCount > 3)
        {
          uint8_t listIdx = abs(atoi(command.GetArg(1)));//String(command.GetArg(1)).toInt());
          uint8_t action = abs(atoi(command.GetArg(2)));//String(command.GetArg(2)).toInt());
          uint8_t param = abs(atoi(command.GetArg(3)));//String(command.GetArg(3)).toInt());
          
          AddCommand(listIdx,action,param); // добавляем команду

            if(wantAnswer)
              PublishSingleton = REG_SUCC; // говорим, что добавили

           PublishSingleton.Status = true;
        }
        else
        {
          // не хватает аргументов
            if(wantAnswer)
              PublishSingleton = PARAMS_MISSED;
        }
      } // CC_ADD_COMMAND
     
      
    } // else
  } // ctSET 
  
  // отвечаем на команду
  mainController->Publish(this,command);
    
  return PublishSingleton.Status;
  
}

