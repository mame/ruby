#include "ruby/ruby.h"
#include "vm_core.h"
#include "id_table.h"

#ifndef GUILD_CHECK_MODE
#define GUILD_CHECK_MODE (1 || RUBY_DEBUG)
#endif

rb_guild_t *rb_guild_main_alloc(void);
void rb_guild_main_setup(rb_guild_t *main_guild);

VALUE rb_guild_self(const rb_guild_t *g);
void rb_guild_atexit(rb_execution_context_t *ec, VALUE result);
void rb_guild_atexit_exception(rb_execution_context_t *ec);
void rb_guild_recv_parameters(rb_execution_context_t *ec, rb_guild_t *g, int len, VALUE *ptr);
void rb_guild_send_parameters(rb_execution_context_t *ec, rb_guild_t *g, VALUE args);

int rb_guild_main_p(void);

VALUE rb_thread_create_guild(rb_guild_t *g, VALUE args, VALUE proc); // defined in thread.c

// TODO: deep frozen
#define RB_OBJ_SHAREABLE_P(obj) FL_TEST_RAW((obj), RUBY_FL_SHAREABLE)

bool rb_guild_shareable_p_continue(VALUE obj);

static inline bool
rb_guild_shareable_p(VALUE obj)
{
    if (SPECIAL_CONST_P(obj)) {
        return true;
    }
    else if (RB_OBJ_SHAREABLE_P(obj)) {
        return true;
    }
    else {
        return rb_guild_shareable_p_continue(obj);
    }
}

#if GUILD_CHECK_MODE > 0

uint32_t rb_guild_id(const rb_guild_t *g);
uint32_t rb_guild_current_id(void);

static inline void
rb_guild_setup_belonging(VALUE obj)
{
    VALUE flags = RBASIC(obj)->flags & 0xffffffff; // 4B
    RBASIC(obj)->flags = flags | ((VALUE)rb_guild_current_id() << 32);
}

static inline uint32_t
rb_guild_belonging(VALUE obj)
{
    if (rb_guild_shareable_p(obj)) {
        return 0;
    }
    else {
        return RBASIC(obj)->flags >> 32;
    }
}

static inline VALUE
rb_guild_confirm_belonging(VALUE obj)
{
    uint32_t id = rb_guild_belonging(obj);

    if (id == 0) {
        if (!rb_guild_shareable_p(obj)) {
            rp(obj);
            rb_bug("id == 0 but not shareable");
        }
    }
    else if (id != rb_guild_current_id()) {
        rb_bug("rb_guild_confirm_belonging object-guild id:%u, current-guild id:%u", id, rb_guild_current_id());
    }
    return obj;
}
#else
#define rb_guild_confirm_belonging(obj) obj
#endif
