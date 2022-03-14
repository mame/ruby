#if defined(__cplusplus)
#include <re2/re2.h>
typedef RE2 rb_re2_regex_t;
#else
// opaque struct
typedef struct rb_re2_regex_t rb_re2_regex_t;
#endif

// compatible with "struct re_registers"
typedef struct {
    int  allocated;
    int  num_regs;
    ptrdiff_t *beg;
    ptrdiff_t *end;
} rb_re2_match_data_t;

#if defined(__cplusplus)
extern "C" {
#if 0
} /* satisfy cc-mode */
#endif
#endif

#define RB_RE2_OPTIONS_IGNORECASE 1
#define RB_RE2_OPTIONS_MULTILINE 2
#define RB_RE2_OPTIONS_BINARY 4

void rb_re2_init(int (*resize_)(rb_re2_match_data_t *mdata, int n), void (*free_)(rb_re2_match_data_t *mdata));

int rb_re2_new(rb_re2_regex_t **reg, const char *pattern, const char *pattern_end, int options, char *err, int err_len_max);
void rb_re2_free(rb_re2_regex_t *reg);
int rb_re2_options(rb_re2_regex_t *reg);
ptrdiff_t rb_re2_search(rb_re2_regex_t *reg, const char *str, const char *end, const char *start, const char *range, rb_re2_match_data_t *mdata);
ptrdiff_t rb_re2_match(rb_re2_regex_t *reg, const char *str, const char *end, const char *at, rb_re2_match_data_t *mdata);

#if defined(__cplusplus)
#if 0
{ /* satisfy cc-mode */
#endif
}  /* extern "C" { */
#endif
