#ifndef  GENERAL_UI_HPP
# define GENERAL_UI_HPP

# include "globals.hpp"
# include <panda3d/pandaFramework.h>
# include <panda3d/rocketRegion.h>
# include "ui/rocket_extension.hpp"
# include "observatory.hpp"
# include "game_console.hpp"
# include "game_options.hpp"

class GeneralUi
{
public:
  GeneralUi(WindowFramework*);
  ~GeneralUi();

  PT(RocketRegion)       GetRocketRegion(void)    const { return (_rocket);   }
  WindowFramework*       GetWindowFramework(void) const { return (_window);   }
  GameOptions&           GetOptions(void)         const { return (*_options); }
  GameConsole&           GetConsole(void)         const { return (*_console); }

private:
  WindowFramework*       _window;
  PT(RocketRegion)       _rocket;
  PT(RocketInputHandler) _ih;

  GameConsole*           _console;
  GameOptions*           _options;
};

#endif
