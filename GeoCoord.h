#pragma once

#include <cmath>

class GeoCoord
{
    float m_lat;
    float m_lon;

public:
    GeoCoord(float lat, float lon)
            : m_lat{lat}, m_lon{lon}
    {}

    float latitude() const { return m_lat; }
    float longitude() const { return m_lon; }
    void setLatitude(float latitude) { m_lat = latitude; }
    void setLongitude(float longitude) { m_lon = longitude; }

    bool isValidLat() const
    {
        return m_lat >= -90.0 && m_lat <= 90.0;
    }

    bool isValidLon() const
    {
        return m_lon >= -180.0 && m_lon < 180.0;
    }

    bool isValid() const
    {
        return isValidLat() && isValidLon();
    }

    // lon可以是任意浮点数
    static float normalizeLongitude(float lon)
    {
        lon = std::fmodf(lon + 180.f, 360.f);
        if (lon < 0)
            lon += 360;
        return lon - 180;
    }

    // lon1/lon2可以是任意浮点数
    static float deltaLongitude(float lon1, float lon2)
    {
        return normalizeLongitude(lon2 - lon1);
    }
};
