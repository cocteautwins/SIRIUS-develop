#include "k_point.h"

namespace sirius {

int K_point::num_occupied_bands(int ispn__)
{
    int nbnd = 0;

    if (ctx_.num_mag_dims() == 3)
    {
        for (int j = 0; j < ctx_.num_bands(); j++)
        {
            if (band_occupancy(j) * weight() > 1e-14) nbnd++;
        }
        return nbnd;
    }

    if (!(ispn__ == 0 || ispn__ == 1)) TERMINATE("wrong spin channel");

    for (int i = 0; i < ctx_.num_fv_states(); i++)
    {
        int j = i + ispn__ * ctx_.num_fv_states();
        if (band_occupancy(j) * weight() > 1e-14) nbnd++;
    }
    return nbnd;
}

};
