#include "HC.h"

#include "RC_Channel.h"

// defining these two macros and including the RC_Channels_VarInfo
// header defines the parameter information common to all vehicle
// types
#define RC_CHANNELS_SUBCLASS RC_Channels_HC
#define RC_CHANNEL_SUBCLASS RC_Channel_HC

#include <RC_Channel/RC_Channels_VarInfo.h>

// note that this callback is not presently used on Plane:
int8_t RC_Channels_HC::flight_mode_channel_number() const
{
    return 1; // HC does not have a flight mode channel
}
