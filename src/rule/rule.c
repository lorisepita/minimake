#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wait.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>

#include <minimake.h>
#include <rule/rule.h>

static struct rule *rule_search(const char *target)
{
    struct rule *rule;
    for (struct _linked *l = g_parsed->rules.head; l; l = l->next)
    {
        rule = l->data;
        if (!strcmp(target, rule->target))
            return rule;
    }
    return NULL;
}

int rule_assign(char *target, struct linked *dependencies,
        struct linked *commands)
{
    struct rule *rule = rule_search(target);
    if (!rule)
    {
        rule = linked_allocate(&g_parsed->rules,
                sizeof(struct rule));
        if (!rule)
        {
            linked_free(commands, NULL);
            linked_free(dependencies, NULL);
            return 0;
        }
    }
    else
    {
        free(&rule->target);
        linked_free(&rule->dependencies, NULL);
        linked_free(&rule->commands, NULL);
    }
    rule->target = target;
    rule->commands.head = commands->head;
    rule->commands.tail = commands->tail;
    rule->dependencies.head = dependencies->head;
    rule->dependencies.tail = dependencies->tail;
    rule->is_built = 0;
    return 1;
}

void rule_free(void *rule_ptr)
{
    struct rule *rule = rule_ptr;
    free(rule->target);
    linked_free(&rule->commands, NULL);
    linked_free(&rule->dependencies, NULL);
    free(rule);
}

static int command_exec(void **cmd)
{
    int status;
    int pid = fork();
    if (pid == -1)
        return -1;
    if (pid)
        waitpid(pid, &status, 0);
    else
    {
        char *args[] = { "/bin/sh", "-c", *cmd, NULL };
        *cmd = NULL;
        if (execvp(args[0], args))
        {
            free(args[2]);
            err(-1, "execve failed");
        }
    }
    return status;
}

static int rule_exec(struct rule *rule)
{
    rule->is_built = 1;
    int res = 0;
    char **str;
    for (struct _linked *command = rule->commands.head; command;
            command = command->next)
    {
        res = 1;
        str = (char **)&command->data;
        int res = variable_expand(str, 1);
        switch (res)
        {
            case 0:
                break;
            case 1:
                err(ERR_BAD_ALLOC, "*** allocation error.  Stop");
            case 2:
                err(ERR_BAD_VAR, "*** unterminated variable reference.  Stop");
            case 3:
                err(ERR_RECURSIVE_VAR,
                    "*** Recursive variable references itself (eventually)."
                    "  Stop.");
            default:
                break;
        }
        if (**str != '@')
            puts(*str);
        fflush(stdout);
        command_exec(&command->data);
    }
    return res;
}

static int timespec_compare(struct timespec *a, struct timespec *b)
{
    if (a->tv_sec < b->tv_sec)
        return 0;
    if (a->tv_sec == b->tv_sec)
        return a->tv_nsec > b->tv_nsec;
    return 1;
}

static void timespec_max(struct timespec *a, struct timespec *b)
{
    if (!timespec_compare(a, b))
        memcpy(a, b, sizeof(struct timespec));
}

static void get_result(int return_code, int *built, int *exec)
{
    *built = (return_code & BUILT) | *built;
    *exec = (return_code & EXEC) | *exec;
}

static int _exec(const char *target, int top)
{
    int exec = 0;
    int built = 0;
    struct rule *rule;
    struct stat statbuf;
    struct timespec recent = { 0, 0 };
    rule = rule_search(target);
    if (!rule)
    {
        if (top)
            errx(ERR_NO_RULE, "*** No rule to make target '%s'."
                    "  Stop.", target);
        return 0;
    }
    if (rule->is_built)
    {
        if (top)
        {
            rule->commands.head
                ? printf("minimake: '%s' is up to date.\n", target)
                : printf("minimake: Nothing to be done for '%s'.\n", target);
        }
       return 0;
    }
    for (struct _linked *dependencies = rule->dependencies.head;
            dependencies; dependencies = dependencies->next)
    {
        char *dep = dependencies->data;
        get_result(_exec(dep, 0), &built, &exec);
        if (!built)
        {
            if (stat(dep, &statbuf))
                errx(ERR_NO_RULE, "*** No rule to make target '%s', "
                        "needed, by '%s'.  Stop.", dep, target);
            timespec_max(&recent, &statbuf.st_mtim);
        }
    }
    if (built || stat(target, &statbuf) ||
            timespec_compare(&recent, &statbuf.st_mtim))
    {
        exec = exec | (rule_exec(rule) ? EXEC : 0);
        if (!exec && top)
            printf("minimake: Nothing to be done for '%s'.\n", target);
        built = 1;
        return built | exec;
    }
    if (top)
        printf("minimake: Nothing to be done for '%s'.\n", target);
    return built | exec;
}

void exec(char *targets[])
{
    if (!*targets)
    {
        struct rule *rule = g_parsed->rules.head->data;
        _exec(rule->target, 1);
        return;
    }
    for (size_t i = 0; targets[i]; ++i)
        _exec(targets[i], 1);
}
