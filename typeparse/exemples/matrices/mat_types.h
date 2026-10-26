/* Produit par type {c!}export : little-endian, pointeurs de 8 octets. Ne pas modifier : regénérer. */
#include <stdint.h>
#include <stddef.h>

/* ligne3 */
typedef double ligne3[3];
_Static_assert(sizeof(ligne3) == 24, "ligne3: size");

/* matrix3 */
typedef ligne3 matrix3[3];
_Static_assert(sizeof(matrix3) == 72, "matrix3: size");

