#pragma once

typedef struct Vector2
{
    union
    {
        struct
        {
            float x;
            float y;
        };
        float value[2];
    };
} Vector2;

typedef struct Rect
{
    union
    {
        struct
        {
            float x;
            float y;
            float w;
            float h;
        };
        float data[4];
    };

    // center-based
    bool contains(const Vector2 &point) const
    {
        return (point.x >= x - w && point.x  < x + w &&
                point.y >= y - h && point.y  < y + h);
    }
    
    /*
    // corner-based
    bool contains(const Vector2& point) const
    {
        return (point.x >= x && point.x <= x + w &&
                point.y >= y && point.y <= y + h);
    }
    */

    bool intersects(const Rect &range) const
    {
        return !(range.x - range.w > x + w ||
            range.x + range.w < x - w ||
            range.y - range.h > y + h ||
            range.y + range.h < y - h);
    }
} Rect;

