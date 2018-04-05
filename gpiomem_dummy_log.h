#ifndef GPIOMEM_DUMMY_LOG_H_GUARD
#define GPIOMEM_DUMMY_LOG_H_GUARD

#define LOG_STRINGIFY_(arg) #arg

#ifdef pr_fmt
#undef pr_fmt
#endif

#define pr_fmt(fmt) __BASE_FILE__ ":" LOG_STRINGIFY_(__FUNCTION__) ":" LOG_STRINGIFY_(__LINE__) ": " fmt "\n"

#include <linux/printk.h>

#endif /* GPIOMEM_DUMMY_LOG_H_GUARD */
