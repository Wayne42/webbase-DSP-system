#pragma once 
#include <Arduino.h>

class Filter
{
    private:
        String name; 
        int min; 
        int max; 

    public: 
        Filter(String n, int mi, int ma):name(n), min(mi), max(ma){}

        String Render(){

        }

};