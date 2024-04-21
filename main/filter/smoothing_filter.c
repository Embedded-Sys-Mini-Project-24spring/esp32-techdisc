#include "smoothing_filter.h"
#include <stdio.h>

void FilterData( int16_t* data, uint8_t dataSize, int16_t* filteredData )
{
    if(dataSize != 5)
    {
        printf("Invalid number of samples attempted to be filtered.");
    }
    else
    {
        //Turn everything into fixed point values
        int32_t tempFilteredValue = ( (-3*(((int32_t)data[0])<<16)) + (12*(((int32_t)data[1])<<16)) + (17*(((int32_t)data[2])<<16)) + (12*(((int32_t)data[3])<<16)) + (-3*(((int32_t)data[4])<<16)) )/35;

        *filteredData = (int16_t)(tempFilteredValue >> 16);
    }
}

void FilterDataFloating( double* data, uint8_t dataSize, double* filteredData )
{
    if(dataSize != 5)
    {
        printf("Invalid number of samples attempted to be filtered.");
    }
    else
    {
        //Filter the data
        *filteredData = ( (-3*data[0]) + (12*data[1]) + (17*data[2]) + (12*data[3]) + (-3*data[4]) )/35;
    }
}