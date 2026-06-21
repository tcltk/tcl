#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <tcl.h>
#include <zlib.h>

/*
 * Security invariant: Decompressing data through Tcl's zlib channel transform
 * must not allow unbounded memory consumption. The expansion ratio of
 * decompressed output to compressed input must be bounded, or the total
 * decompressed output must be capped.
 *
 * We test this by creating a zlib decompress channel and feeding it a small
 * compressed payload that expands to a large size, verifying that either
 * the operation is bounded or returns an error before exhausting memory.
 */

#define MAX_DECOMPRESSED_BYTES (64 * 1024 * 1024)  /* 64 MB safety limit */
#define MAX_EXPANSION_RATIO 1000

START_TEST(test_zlib_transform_bounded_expansion)
{
    Tcl_Interp *interp = Tcl_CreateInterp();
    ck_assert_ptr_nonnull(interp);

    /* Create a highly compressible payload: 1MB of zeros compresses tiny */
    size_t raw_size = 1024 * 1024;
    unsigned char *raw = calloc(raw_size, 1);
    ck_assert_ptr_nonnull(raw);

    uLongf comp_size = compressBound(raw_size);
    unsigned char *compressed = malloc(comp_size);
    ck_assert_ptr_nonnull(compressed);

    int zrc = compress2(compressed, &comp_size, raw, raw_size, 9);
    ck_assert_int_eq(zrc, Z_OK);
    free(raw);

    /* Write compressed data to a temp file, then decompress via Tcl zlib channel */
    const char *tmpfile = "zlib_test_bomb.z";
    FILE *f = fopen(tmpfile, "wb");
    ck_assert_ptr_nonnull(f);
    fwrite(compressed, 1, comp_size, f);
    fclose(f);
    free(compressed);

    /* Use Tcl to open and decompress - check total bytes read is bounded */
    char script[1024];
    snprintf(script, sizeof(script),
        "set fd [open %s rb]\n"
        "zlib push decompress $fd\n"
        "set total 0\n"
        "while {![eof $fd]} {\n"
        "  set chunk [read $fd 65536]\n"
        "  incr total [string length $chunk]\n"
        "  if {$total > %d} { close $fd; error \"expansion_exceeded\" }\n"
        "}\n"
        "close $fd\n"
        "set total", tmpfile, MAX_DECOMPRESSED_BYTES);

    int rc = Tcl_Eval(interp, script);

    if (rc == TCL_OK) {
        long total = 0;
        Tcl_GetLongFromObj(NULL, Tcl_GetObjResult(interp), &total);
        /* Invariant: expansion ratio must be reasonable */
        ck_assert_msg((size_t)total <= MAX_DECOMPRESSED_BYTES,
            "Decompressed %ld bytes exceeds safety limit", total);
        if (comp_size > 0) {
            long ratio = total / (long)comp_size;
            ck_assert_msg(ratio <= MAX_EXPANSION_RATIO,
                "Expansion ratio %ld exceeds maximum %d", ratio, MAX_EXPANSION_RATIO);
        }
    }
    /* If TCL_ERROR with "expansion_exceeded", the script-level guard caught it,
     * which demonstrates the vulnerability (no built-in protection). */
    if (rc == TCL_ERROR) {
        const char *result = Tcl_GetStringResult(interp);
        if (strstr(result, "expansion_exceeded")) {
            /* This means the native layer did NOT enforce a limit - flag it */
            ck_abort_msg("Zlib transform allowed unbounded expansion; "
                         "no native decompression limit enforced");
        }
    }

    remove(tmpfile);
    Tcl_DeleteInterp(interp);
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_set_timeout(tc_core, 30);
    tcase_add_test(tc_core, test_zlib_transform_bounded_expansion);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{