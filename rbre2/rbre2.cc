#include "rbre2.h"

static int (*match_data_resize)(rb_re2_match_data_t *mdata, int n);
static void (*match_data_free)(rb_re2_match_data_t *mdata);

void
rb_re2_init(int (*resize_)(rb_re2_match_data_t *mdata, int n), void (*free_)(rb_re2_match_data_t *mdata))
{
    match_data_resize = resize_;
    match_data_free = free_;
}

#define CHECK_BUF(n) {                      \
    int len = q - dst;                      \
    if (len + (n) >= dst_len) {             \
        while (len + (n) >= dst_len) {      \
            dst_len *= 2;                   \
        }                                   \
        q = (char *) realloc(dst, dst_len); \
        if (!q) {                           \
            free(dst);                      \
            return 0;                       \
        }                                   \
        dst = q;                            \
        q = dst + len;                      \
    }                                       \
}

#define COPY {      \
    CHECK_BUF(1)    \
    *q++ = *p++;    \
}

#define APPEND(s) {  \
    CHECK_BUF(sizeof(s) - 1);       \
    memcpy(q, s, sizeof(s) - 1);    \
    q += sizeof(s) - 1;             \
}

static int
preprocess_regexp(const char *src, const char *src_end, const char **dst_p, const char **dst_end_p, int ascii)
{
    size_t dst_len;
    const char *p;
    char *dst, *q;
    int in_class = 0;

    dst_len = src_end - src + 10;
    dst = (char *)malloc(dst_len);
    if (!dst) return 0;

    memcpy(dst, "(?m)", 4);

    p = src;
    q = dst + 4;
    while (p < src_end) {
        switch (*p) {
            case '[':
                COPY;
                in_class++;
                break;
            case ']':
                COPY;
                in_class--;
                if (in_class < 0) in_class = 0;
                break;
            case '(':
                COPY;
                break;
            case '\\':
                if (p + 1 == src_end) {
                    // XXX: parse error
                    COPY;
                    break;
                }
                switch (p[1]) {
                    case 'b':
                        goto fail;
                    case 'e':
                        APPEND("\\x1b");
                        p += 2;
                        break;
                    case 's':
                        if (in_class) {
                            if (ascii) APPEND("\\t\\n\\v\\f\\r\\x20")
                            else APPEND("\\t\\n\\v\\f\\r\\x{0085}\\p{Zl}\\p{Zp}\\p{Zs}");
                        }
                        else {
                            if (ascii) APPEND("[\\t\\n\\v\\f\\r\\x20]")
                            else APPEND("[\\t\\n\\v\\f\\r\\x{0085}\\p{Zl}\\p{Zp}\\p{Zs}]");
                        }
                        p += 2;
                        break;
                    case 'S':
                        if (in_class) {
                            goto fail;
                        }
                        else {
                            if (ascii) APPEND("[^\\t\\n\\v\\f\\r\\x20]")
                            else APPEND("[^\\t\\n\\v\\f\\r\\x{0085}\\p{Zl}\\p{Zp}\\p{Zs}]");
                        }
                        p += 2;
                        break;
                    default:
                        COPY;
                        COPY;
                }
                break;
            default:
                COPY;
        }
    }

    *dst_p = dst;
    *dst_end_p = q;
    //*q = '\0';
    //puts(dst);

    return 1;

fail:
    free(dst);
    return 0;
}

int
rb_re2_new(rb_re2_regex_t **reg, const char *pattern, const char *pattern_end, int options, char *err, int err_len_max)
{
    const char *pat, *pat_end;
    if (preprocess_regexp(pattern, pattern_end, &pat, &pat_end, options & RB_RE2_OPTIONS_BINARY) == 0) {
        strcpy(err, "failed to preprocess");
        return -1;
    }

    re2::StringPiece re2_input(pat, pat_end - pat);

    RE2::Options opts;
    opts.set_encoding(options & RB_RE2_OPTIONS_BINARY ? RE2::Options::EncodingLatin1 : RE2::Options::EncodingUTF8);
    opts.set_case_sensitive(options & RB_RE2_OPTIONS_IGNORECASE ? false : true);
    opts.set_dot_nl(options & RB_RE2_OPTIONS_MULTILINE ? true : false);
    opts.set_log_errors(false);

    RE2 *r = new RE2(re2_input, opts);
    if (r->ok()) {
        *reg = r;
        return 0;
    }
    else {
        std::string msg = r->error();
        strncpy(err, msg.c_str(), err_len_max - 1);
        err[err_len_max - 1] = 0;
        delete r;
        return -1;
    }
}

void
rb_re2_free(rb_re2_regex_t *reg)
{
    delete reg;
}

int
rb_re2_options(rb_re2_regex_t *reg) {
    const RE2::Options *opts = &reg->options();
    int ret = 0;
    if (opts->encoding() == RE2::Options::EncodingLatin1) ret |= RB_RE2_OPTIONS_BINARY;
    if (!opts->case_sensitive()) ret |= RB_RE2_OPTIONS_IGNORECASE;
    if (opts->dot_nl()) ret |= RB_RE2_OPTIONS_MULTILINE;
    return ret;
}

void
rb_re2_code_to_str(rb_re2_regex_t *reg)
{
    std::string err = reg->error();
}

static ptrdiff_t
match(rb_re2_regex_t *reg, const char *str, const char *end, const char *start, const char *range, re2::RE2::Anchor anchor, rb_re2_match_data_t *mdata)
{
    re2::StringPiece text(str, end - str);

    int n = mdata ? reg->NumberOfCapturingGroups() + 1 : 0;
    RE2::Arg *args[n];
    RE2::Arg arg_bodies[n];
    re2::StringPiece results[n];

    if (reg->Match(text, start - str, range - str, anchor, results, n)) {
        if (mdata) match_data_resize(mdata, n);
        for (std::size_t i = 0; i < n; i++) {
            const char *data = results[i].data();
            mdata->beg[i] = data ? data - str : -1;
            mdata->end[i] = data ? mdata->beg[i] + results[i].size() : -1;
        }
        return n ? results[0].data() - str : 0;
    }
    else {
        return -1; // ONIG_MISMATCH
    }
}


ptrdiff_t
rb_re2_match(rb_re2_regex_t *reg, const char *str, const char *end, const char *at, rb_re2_match_data_t *mdata)
{
    return match(reg, str, end, at, end, RE2::ANCHOR_BOTH, mdata);
}

ptrdiff_t
rb_re2_search(rb_re2_regex_t *reg, const char *str, const char *end, const char *start, const char *range, rb_re2_match_data_t *mdata)
{
    return match(reg, str, end, start, range, RE2::UNANCHORED, mdata);
}
