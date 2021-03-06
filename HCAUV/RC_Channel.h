#pragma once

#include <RC_Channel/RC_Channel.h>

class RC_Channel_HC : public RC_Channel
{

public:

protected:

private:

};

class RC_Channels_HC : public RC_Channels
{
public:

    RC_Channel_HC obj_channels[NUM_RC_CHANNELS];
    RC_Channel_HC *channel(const uint8_t chan) override {
        if (chan > NUM_RC_CHANNELS) {
            return nullptr;
        }
        return &obj_channels[chan];
    }

protected:

    // note that these callbacks are not presently used on Plane:
    int8_t flight_mode_channel_number() const override;

};
