#include "platform/sdl/navkey_sdl.h"
#include <SDL_webOS.h>

NAVKEY navkey_from_sdl_webos(SDL_Event ev)
{
    switch (ev.type)
    {
    case SDL_KEYDOWN:
    case SDL_KEYUP:
    {
        switch (ev.key.keysym.scancode)
        {
        case SDL_WEBOS_SCANCODE_RED:
            return NAVKEY_NEGATIVE;
        case SDL_WEBOS_SCANCODE_YELLOW:
            return NAVKEY_MENU;
        case SDL_WEBOS_SCANCODE_BACK:
            return NAVKEY_CANCEL;
        default:
            break;
        }
    }
    default:
        return NAVKEY_UNKNOWN;
    }
}