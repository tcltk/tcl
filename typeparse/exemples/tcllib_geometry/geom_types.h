/* Produit par type {c!}export : little-endian, pointeurs de 8 octets. Ne pas modifier : regénérer. */
#include <stdint.h>
#include <stddef.h>

/* point */
typedef struct {
    double x;
    double y;
} point;
_Static_assert(sizeof(point) == 16, "point: size");
_Static_assert(offsetof(point, x) == 0, "point.x: offset");
_Static_assert(offsetof(point, y) == 8, "point.y: offset");

/* line */
typedef struct {
    double x1;
    double y1;
    double x2;
    double y2;
} line;
_Static_assert(sizeof(line) == 32, "line: size");
_Static_assert(offsetof(line, x1) == 0, "line.x1: offset");
_Static_assert(offsetof(line, y1) == 8, "line.y1: offset");
_Static_assert(offsetof(line, x2) == 16, "line.x2: offset");
_Static_assert(offsetof(line, y2) == 24, "line.y2: offset");

