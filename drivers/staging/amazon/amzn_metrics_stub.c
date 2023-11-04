#include <linux/module.h>

int log_to_metrics(int priority, const char *domain, char *log_msg)
{
    return 0;
}
EXPORT_SYMBOL_GPL(log_to_metrics);

int log_timer_to_vitals(int priority,
			const char *domain, const char *program,
			const char *source, const char *key,
			long timer_value, const char *unit, int type)
{
    return 0;
}
EXPORT_SYMBOL_GPL(log_timer_to_vitals);

int log_counter_to_vitals(int priority,
                          const char *domain, const char *program,
                        const char *source, const char *key,
                        long counter_value, const char *unit,
                        const char *metadata, int type)
{
    return 0;
}
EXPORT_SYMBOL_GPL(log_counter_to_vitals);

int minerva_metrics_log(char *buf, int max_size, char *fmt, ...)
{
    return 0;
}
EXPORT_SYMBOL_GPL(minerva_metrics_log);

int minerva_timer_to_vitals(int priority,
			const char *group_id, const char *schema_id,
			const char *domain, const char *program,
			const char *source, const char *key,
			long timer_value, const char *unit, int type,
			const char *dimensions, const char *annotations)
{
    return 0;
}
EXPORT_SYMBOL_GPL(minerva_timer_to_vitals);

int minerva_counter_to_vitals(int priority,
			const char *group_id, const char *schema_id,
			const char *domain, const char *program,
			const char *source, const char *key,
			long counter_value, const char *unit,
			const char *metadata, int type,
			const char *dimensions, const char *annotations)
{
    return 0;
}
EXPORT_SYMBOL_GPL(minerva_counter_to_vitals);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Amazon Metrics stub driver");
