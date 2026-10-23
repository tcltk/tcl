/* Produit par type c : little-endian, pointeurs de 8 octets. Ne pas modifier : regénérer. */
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

/* pointC */
typedef struct {
    double x;
    double y;
} pointC;
_Static_assert(sizeof(pointC) == 16, "pointC: size");
_Static_assert(offsetof(pointC, x) == 0, "pointC.x: offset");
_Static_assert(offsetof(pointC, y) == 8, "pointC.y: offset");

/* polyC */
typedef struct {
    uint32_t n;
    pointC pts[];
} polyC;
_Static_assert(sizeof(polyC) >= 8, "polyC: size");
_Static_assert(offsetof(polyC, n) == 0, "polyC.n: offset");
_Static_assert(offsetof(polyC, pts) == 8, "polyC.pts: offset");

