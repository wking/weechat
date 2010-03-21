/*
 * Copyright (c) 2003-2010 by FlashCode <flashcode@flashtux.org>
 * See README for License detail, AUTHORS for developers list.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* alias.c: Alias plugin for WeeChat */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../weechat-plugin.h"
#include "alias.h"
#include "alias-config.h"
#include "alias-info.h"


WEECHAT_PLUGIN_NAME(ALIAS_PLUGIN_NAME);
WEECHAT_PLUGIN_DESCRIPTION("Alias plugin for WeeChat");
WEECHAT_PLUGIN_AUTHOR("FlashCode <flashcode@flashtux.org>");
WEECHAT_PLUGIN_VERSION(WEECHAT_VERSION);
WEECHAT_PLUGIN_LICENSE("GPL3");

#define ALIAS_IS_ARG_NUMBER(number) ((number >= '1') && (number <= '9'))

struct t_weechat_plugin *weechat_alias_plugin = NULL;

struct t_alias *alias_list = NULL;
struct t_alias *last_alias = NULL;


/*
 * alias_valid: check if an alias pointer exists
 *              return 1 if alias exists
 *                     0 if alias is not found
 */

int
alias_valid (struct t_alias *alias)
{
    struct t_alias *ptr_alias;
    
    if (!alias)
        return 0;
    
    for (ptr_alias = alias_list; ptr_alias;
         ptr_alias = ptr_alias->next_alias)
    {
        if (ptr_alias == alias)
            return 1;
    }
    
    /* alias not found */
    return 0;
}

/*
 * alias_search: search an alias
 */

struct t_alias *
alias_search (const char *alias_name)
{
    struct t_alias *ptr_alias;
    
    for (ptr_alias = alias_list; ptr_alias;
         ptr_alias = ptr_alias->next_alias)
    {
        if (weechat_strcasecmp (alias_name, ptr_alias->name) == 0)
            return ptr_alias;
    }
    return NULL;
}

/*
 * alias_string_add_word: add word to string and increment length
 */

void
alias_string_add_word (char **alias, int *length, const char *word)
{
    int length_word;

    if (!word)
        return;
    
    length_word = strlen (word);
    if (length_word == 0)
        return;
    
    if (*alias == NULL)
    {
        *alias = malloc (length_word + 1);
        strcpy (*alias, word);
    }
    else
    {
        *alias = realloc (*alias, strlen (*alias) + length_word + 1);
        strcat (*alias, word);
    }
    *length += length_word;
}

/*
 * alias_string_add_word_range: add word (in range) to string and increment
 *                              length
 */

void
alias_string_add_word_range (char **alias, int *length, const char *start,
                             const char *end)
{
    char *word;
    
    word = weechat_strndup (start, end - start);
    if (word)
    {
        alias_string_add_word (alias, length, word);
        free (word);
    }
}

/*
 * alias_string_add_arguments: add some arguments to string and increment
 *                             length
 */

void
alias_string_add_arguments (char **alias, int *length, char **argv, int start,
                            int end)
{
    int i;
    
    for (i = start; i <= end; i++)
    {
        if (i != start)
            alias_string_add_word (alias, length, " ");
        alias_string_add_word (alias, length, argv[i]);
    }
}

/*
 * alias_replace_args: replace arguments in alias
 *                     arguments are:
 *                       $n   argument n
 *                       $-m  arguments from 1 to m
 *                       $n-  arguments from n to last
 *                       $n-m arguments from n to m
 *                       $*   all arguments
 *                       $~   last argument
 *                     with n and m in 1..9
 */

char *
alias_replace_args (const char *alias_args, const char *user_args)
{
    char **argv, *res;
    const char *start, *pos;
    int n, m, argc, length_res, args_count, offset;
    
    argv = weechat_string_split (user_args, " ", 0, 0, &argc);
    
    res = NULL;
    length_res = 0;
    args_count = 0;
    start = alias_args;
    pos = start;
    while (pos && pos[0])
    {
        offset = 0;
        
        if ((pos[0] == '\\') && (pos[1] == '$'))
        {
            offset = 2;
            alias_string_add_word_range (&res, &length_res, start, pos);
            alias_string_add_word (&res, &length_res, "$");
        }
        else
        {
            if (pos[0] == '$')
            {
                if (pos[1] == '*')
                {
                    /* replace with all arguments */
                    args_count++;
                    offset = 2;
                    if (pos > start)
                        alias_string_add_word_range (&res, &length_res, start, pos);
                    alias_string_add_word (&res, &length_res, user_args);
                }
                else if (pos[1] == '~')
                {
                    /* replace with last argument */
                    args_count++;
                    offset = 2;
                    if (pos > start)
                        alias_string_add_word_range (&res, &length_res, start, pos);
                    if (argc > 0)
                        alias_string_add_word (&res, &length_res, argv[argc - 1]);
                }
                else if ((pos[1] == '-') && ALIAS_IS_ARG_NUMBER(pos[2]))
                {
                    /* replace with arguments 1 to m */
                    args_count++;
                    offset = 3;
                    if (pos > start)
                        alias_string_add_word_range (&res, &length_res, start, pos);
                    if (pos[2] - '1' < argc)
                        m = pos[2] - '1';
                    else
                        m = argc - 1;
                    alias_string_add_arguments (&res, &length_res, argv, 0, m);
                }
                else if (ALIAS_IS_ARG_NUMBER(pos[1]))
                {
                    args_count++;
                    n = pos[1] - '1';
                    if (pos > start)
                        alias_string_add_word_range (&res, &length_res, start, pos);
                    if (pos[2] != '-')
                    {
                        /* replace with argument n */
                        offset = 2;
                        if (n < argc)
                            alias_string_add_word (&res, &length_res, argv[n]);
                    }
                    else
                    {
                        if (ALIAS_IS_ARG_NUMBER(pos[3]))
                        {
                            /* replace with arguments n to m */
                            offset = 4;
                            if (pos[3] - '1' < argc)
                                m = pos[3] - '1';
                            else
                                m = argc - 1;
                        }
                        else
                        {
                            /* replace with arguments n to last */
                            offset = 3;
                            m = argc - 1;
                        }
                        if (n < argc)
                        {
                            alias_string_add_arguments (&res, &length_res,
                                                        argv, n, m);
                        }
                    }
                }
            }
        }
        
        if (offset != 0)
        {
            pos += offset;
            start = pos;
        }
        else
            pos++;
    }
    
    if (pos > start)
        alias_string_add_word (&res, &length_res, start);
    
    if (argv)
        weechat_string_free_split (argv);
    
    return res;
}

/*
 * alias_run_command: replace local buffer variables in string, then run
 *                    command on buffer
 */

void
alias_run_command (struct t_gui_buffer **buffer, const char *command)
{
    char *string;
    struct t_gui_buffer *old_current_buffer, *new_current_buffer;
    
    /* save current buffer pointer */
    old_current_buffer = weechat_current_buffer();
    
    /* execute command */
    string = weechat_buffer_string_replace_local_var (*buffer, command);
    weechat_command (*buffer,
                     (string) ? string : command);
    if (string)
        free (string);
    
    /* get new current buffer */
    new_current_buffer = weechat_current_buffer();
    
    /*
     * if current buffer was changed by command, then we'll use this one for
     * next commands in alias
     */
    if (old_current_buffer != new_current_buffer)
        *buffer = new_current_buffer;
}

/*
 * alias_cb: callback for alias (called when user uses an alias)
 */

int
alias_cb (void *data, struct t_gui_buffer *buffer, int argc, char **argv,
          char **argv_eol)
{
    struct t_alias *ptr_alias;
    char **commands, **ptr_cmd, **ptr_next_cmd;
    char *args_replaced, *alias_command;
    int some_args_replaced, length1, length2;
    
    /* make C compiler happy */
    (void) argv;
    
    ptr_alias = (struct t_alias *)data;
    
    if (ptr_alias->running)
    {
        weechat_printf (NULL,
                        _("%s%s: error, circular reference when calling "
                          "alias \"%s\""),
                        weechat_prefix ("error"), ALIAS_PLUGIN_NAME,
                        ptr_alias->name);
        return WEECHAT_RC_ERROR;
    }
    else
    {
        /* an alias can contain many commands separated by ';' */
        commands = weechat_string_split_command (ptr_alias->command, ';');
        if (commands)
        {
            some_args_replaced = 0;
            ptr_alias->running = 1;
            for (ptr_cmd = commands; *ptr_cmd; ptr_cmd++)
            {
                ptr_next_cmd = ptr_cmd;
                ptr_next_cmd++;
                
                args_replaced = alias_replace_args (*ptr_cmd,
                                                    (argc > 1) ? argv_eol[1] : "");
                if (args_replaced && (strcmp (args_replaced, *ptr_cmd) != 0))
                    some_args_replaced = 1;
                
                /*
                 * if alias has arguments, they are now
                 * arguments of the last command in the list (if no $1,$2,..$*)
                 * was found
                 */
                if ((*ptr_next_cmd == NULL) && argv_eol[1] && (!some_args_replaced))
                {
                    length1 = strlen (*ptr_cmd);
                    length2 = strlen (argv_eol[1]);
                    
                    alias_command = malloc (1 + length1 + 1 + length2 + 1);
                    if (alias_command)
                    {
                        if (!weechat_string_is_command_char (*ptr_cmd))
                            strcpy (alias_command, "/");
                        else
                            alias_command[0] = '\0';
                        
                        strcat (alias_command, *ptr_cmd);
                        strcat (alias_command, " ");
                        strcat (alias_command, argv_eol[1]);
                        
                        alias_run_command (&buffer,
                                           alias_command);
                        free (alias_command);
                    }
                }
                else
                {
                    if (weechat_string_is_command_char (*ptr_cmd))
                    {
                        alias_run_command (&buffer,
                                           (args_replaced) ? args_replaced : *ptr_cmd);
                    }
                    else
                    {
                        alias_command = malloc (1 + strlen((args_replaced) ? args_replaced : *ptr_cmd) + 1);
                        if (alias_command)
                        {
                            strcpy (alias_command, "/");
                            strcat (alias_command, (args_replaced) ? args_replaced : *ptr_cmd);
                            alias_run_command (&buffer,
                                               alias_command);
                            free (alias_command);
                        }
                    }
                }
                
                if (args_replaced)
                    free (args_replaced);
            }
            ptr_alias->running = 0;
            weechat_string_free_split_command (commands);
        }
    }
    return WEECHAT_RC_OK;
}

/*
 * alias_free: free an alias and remove it from list
 */

void
alias_free (struct t_alias *alias)
{
    struct t_alias *new_alias_list;
    
    /* remove alias from list */
    if (last_alias == alias)
        last_alias = alias->prev_alias;
    if (alias->prev_alias)
    {
        (alias->prev_alias)->next_alias = alias->next_alias;
        new_alias_list = alias_list;
    }
    else
        new_alias_list = alias->next_alias;
    if (alias->next_alias)
        (alias->next_alias)->prev_alias = alias->prev_alias;

    /* free data */
    if (alias->hook)
        weechat_unhook (alias->hook);
    if (alias->name)
        free (alias->name);
    if (alias->command)
        free (alias->command);
    if (alias->completion)
        free (alias->completion);
    free (alias);
    
    alias_list = new_alias_list;
}

/*
 * alias_free_all: free all alias
 */

void
alias_free_all ()
{
    while (alias_list)
    {
        alias_free (alias_list);
    }
}

/*
 * alias_find_pos: find position for an alias (for sorting aliases)
 */

struct t_alias *
alias_find_pos (const char *name)
{
    struct t_alias *ptr_alias;
    
    for (ptr_alias = alias_list; ptr_alias; ptr_alias = ptr_alias->next_alias)
    {
        if (weechat_strcasecmp (name, ptr_alias->name) < 0)
            return ptr_alias;
    }
    
    /* position not found (we will add to the end of list) */
    return NULL;
}

/*
 * alias_hook_command: call weehat_hook_command() for an alias and store result
 *                     in variable "hook" of alias
 */

void
alias_hook_command (struct t_alias *alias)
{
    char *str_completion;
    int length;
    
    str_completion = NULL;
    
    if (!alias->completion)
    {
        /*
         * if alias has no custom completion, then default is to complete with
         * completion template of target command, for example if alias is
         * "/alias test /buffer", then str_completion will be "%%buffer"
         */
        length = 2 + strlen (alias->command) + 1;
        str_completion = malloc (length);
        if (str_completion)
        {
            snprintf (str_completion, length, "%%%%%s",
                      (weechat_string_is_command_char (alias->command)) ?
                      weechat_utf8_next_char (alias->command) : alias->command);
        }
    }
    
    alias->hook = weechat_hook_command (alias->name, alias->command,
                                        NULL, NULL,
                                        (str_completion) ? str_completion : alias->completion,
                                        &alias_cb, alias);
    
    if (str_completion)
        free (str_completion);
}

/*
 * alias_update_completion: update completion for an alias
 */

void
alias_update_completion (struct t_alias *alias, const char *completion)
{
    /* update completion in alias */
    if (alias->completion)
        free (alias->completion);
    alias->completion = (completion) ? strdup (completion) : NULL;
    
    /* unhook and hook again command, with new completion */
    weechat_unhook (alias->hook);
    alias->hook = NULL;
    alias_hook_command (alias);
}

/*
 * alias_new: create new alias and add it to alias list
 */

struct t_alias *
alias_new (const char *name, const char *command, const char *completion)
{
    struct t_alias *new_alias, *ptr_alias, *pos_alias;
    
    if (!name || !name[0] || !command || !command[0])
        return NULL;
    
    while (weechat_string_is_command_char (name))
    {
        name = weechat_utf8_next_char (name);
    }
    
    ptr_alias = alias_search (name);
    if (ptr_alias)
        alias_free (ptr_alias);
    
    new_alias = malloc (sizeof (*new_alias));
    if (new_alias)
    {
        new_alias->hook = NULL;
        new_alias->name = strdup (name);
        new_alias->command = strdup (command);
        new_alias->completion = (completion) ? strdup (completion) : NULL;
        new_alias->running = 0;
        
        alias_hook_command (new_alias);
        
        if (alias_list)
        {
            pos_alias = alias_find_pos (name);
            if (pos_alias)
            {
                /* insert alias into the list (before alias found) */
                new_alias->prev_alias = pos_alias->prev_alias;
                new_alias->next_alias = pos_alias;
                if (pos_alias->prev_alias)
                    (pos_alias->prev_alias)->next_alias = new_alias;
                else
                    alias_list = new_alias;
                pos_alias->prev_alias = new_alias;
            }
            else
            {
                /* add alias to end of list */
                new_alias->prev_alias = last_alias;
                new_alias->next_alias = NULL;
                last_alias->next_alias = new_alias;
                last_alias = new_alias;
            }
        }
        else
        {
            new_alias->prev_alias = NULL;
            new_alias->next_alias = NULL;
            alias_list = new_alias;
            last_alias = new_alias;
        }
    }
    
    return new_alias;
}

/*
 * alias_get_final_command: get final command pointed by an alias
 */

char *
alias_get_final_command (struct t_alias *alias)
{
    struct t_alias *ptr_alias;
    char *result;
    
    if (alias->running)
    {
        weechat_printf (NULL,
                        _("%s%s: error, circular reference when calling "
                          "alias \"%s\""),
                        weechat_prefix ("error"), ALIAS_PLUGIN_NAME,
                        alias->name);
        return NULL;
    }
    
    ptr_alias = alias_search ((weechat_string_is_command_char (alias->command)) ?
                              weechat_utf8_next_char (alias->command) : alias->command);
    if (ptr_alias)
    {
        alias->running = 1;
        result = alias_get_final_command (ptr_alias);
        alias->running = 0;
        return result;
    }
    return (weechat_string_is_command_char (alias->command)) ?
        weechat_utf8_next_char (alias->command) : alias->command;
}

/*
 * alias_command_cb: display or create alias
 */

int
alias_command_cb (void *data, struct t_gui_buffer *buffer, int argc,
                  char **argv, char **argv_eol)
{
    char *ptr_completion, *ptr_alias_name, *ptr_command;
    struct t_alias *ptr_alias;
    struct t_config_option *ptr_option;
    int alias_found;
    
    /* make C compiler happy */
    (void) data;
    (void) buffer;
    
    if (argc > 1)
    {
        if (argc > 2)
        {
            /* get pointers to completion, alias name and command (+ args) */
            ptr_completion = NULL;
            ptr_alias_name = NULL;
            ptr_command = NULL;
            if ((argc > 4) && (weechat_strcasecmp (argv[1], "-completion") == 0))
            {
                ptr_completion = argv[2];
                ptr_alias_name = (weechat_string_is_command_char (argv[3])) ?
                    weechat_utf8_next_char (argv[3]) : argv[3];
                ptr_command = argv_eol[4];
            }
            else
            {
                ptr_alias_name = (weechat_string_is_command_char (argv[1])) ?
                    weechat_utf8_next_char (argv[1]) : argv[1];
                ptr_command = argv_eol[2];
            }
            
            /* define new alias */
            if (!alias_new (ptr_alias_name, ptr_command, ptr_completion))
            {
                weechat_printf (NULL,
                                _("%s%s: error creating alias \"%s\" "
                                  "=> \"%s\""),
                                weechat_prefix ("error"), ALIAS_PLUGIN_NAME,
                                ptr_alias_name, ptr_command);
                return WEECHAT_RC_ERROR;
            }
            
            /* create config option for command */
            ptr_option = weechat_config_search_option (alias_config_file,
                                                       alias_config_section_cmd,
                                                       ptr_alias_name);
            if (ptr_option)
                weechat_config_option_free (ptr_option);
            alias_config_cmd_new_option (ptr_alias_name, ptr_command);
            
            /* create config option for completion */
            ptr_option = weechat_config_search_option (alias_config_file,
                                                       alias_config_section_completion,
                                                       ptr_alias_name);
            if (ptr_option)
                weechat_config_option_free (ptr_option);
            if (ptr_completion)
                alias_config_completion_new_option (ptr_alias_name, ptr_completion);
            
            /* display message */
            weechat_printf (NULL,
                            _("Alias \"%s\" => \"%s\" created"),
                            ptr_alias_name, ptr_command);
        }
        else
        {
            /* get pointer to alias name */
            ptr_alias_name = (weechat_string_is_command_char (argv[1])) ?
                weechat_utf8_next_char (argv[1]) : argv[1];
            
            /* display list of aliases */
            alias_found = 0;
            for (ptr_alias = alias_list; ptr_alias;
                 ptr_alias = ptr_alias->next_alias)
            {
                if (weechat_string_match (ptr_alias->name, ptr_alias_name, 0))
                {
                    if (!alias_found)
                    {
                        weechat_printf (NULL, "");
                        weechat_printf (NULL, _("List of aliases:"));
                    }
                    weechat_printf (NULL, "  %s %s=>%s %s",
                                    ptr_alias->name,
                                    weechat_color ("chat_delimiters"),
                                    weechat_color ("chat"),
                                    ptr_alias->command);
                    alias_found = 1;
                }
            }
            if (!alias_found)
            {
                weechat_printf (NULL, _("No alias found matching \"%s\""),
                                ptr_alias_name);
            }
        }
    }
    else
    {
        /* List all aliases */
        if (alias_list)
        {
            weechat_printf (NULL, "");
            weechat_printf (NULL, _("List of aliases:"));
            for (ptr_alias = alias_list; ptr_alias;
                 ptr_alias = ptr_alias->next_alias)
            {
                weechat_printf (NULL,
                                "  %s %s=>%s %s",
                                ptr_alias->name,
                                weechat_color ("chat_delimiters"),
                                weechat_color ("chat"),
                                ptr_alias->command);
            }
        }
        else
            weechat_printf (NULL, _("No alias defined"));
    }
    
    return WEECHAT_RC_OK;
}

/*
 * unalias_command_cb: remove an alias
 */

int
unalias_command_cb (void *data, struct t_gui_buffer *buffer, int argc,
                    char **argv, char **argv_eol)
{
    int i;
    char *alias_name;
    struct t_alias *ptr_alias;
    struct t_config_option *ptr_option;
    
    /* make C compiler happy */
    (void) data;
    (void) buffer;
    (void) argv_eol;
    
    if (argc > 1)
    {
        for (i = 1; i < argc; i++)
        {
            alias_name = (weechat_string_is_command_char (argv[i])) ?
                weechat_utf8_next_char (argv[i]) : argv[i];
            ptr_alias = alias_search (alias_name);
            if (!ptr_alias)
            {
                weechat_printf (NULL,
                                _("%sAlias \"%s\" not found"),
                                weechat_prefix ("error"),
                                alias_name);
            }
            else
            {
                /* remove alias */
                alias_free (ptr_alias);
                
                /* remove options */
                ptr_option = weechat_config_search_option (alias_config_file,
                                                           alias_config_section_cmd,
                                                           alias_name);
                if (ptr_option)
                    weechat_config_option_free (ptr_option);
                ptr_option = weechat_config_search_option (alias_config_file,
                                                           alias_config_section_completion,
                                                           alias_name);
                if (ptr_option)
                    weechat_config_option_free (ptr_option);
                
                weechat_printf (NULL,
                                _("Alias \"%s\" removed"),
                                alias_name);
            }
        }
    }
    return WEECHAT_RC_OK;
}

/*
 * alias_completion_cb: callback for completion with list of aliases
 */

int
alias_completion_cb (void *data, const char *completion_item,
                     struct t_gui_buffer *buffer,
                     struct t_gui_completion *completion)
{
    struct t_alias *ptr_alias;
    
    /* make C compiler happy */
    (void) data;
    (void) completion_item;
    (void) buffer;
    
    for (ptr_alias = alias_list; ptr_alias;
         ptr_alias = ptr_alias->next_alias)
    {
        weechat_hook_completion_list_add (completion, ptr_alias->name,
                                          0, WEECHAT_LIST_POS_SORT);
    }
    
    return WEECHAT_RC_OK;
}

/*
 * alias_add_to_infolist: add an alias in an infolist
 *                        return 1 if ok, 0 if error
 */

int
alias_add_to_infolist (struct t_infolist *infolist, struct t_alias *alias)
{
    struct t_infolist_item *ptr_item;
    
    if (!infolist || !alias)
        return 0;
    
    ptr_item = weechat_infolist_new_item (infolist);
    if (!ptr_item)
        return 0;
    
    if (!weechat_infolist_new_var_pointer (ptr_item, "hook", alias->hook))
        return 0;
    if (!weechat_infolist_new_var_string (ptr_item, "name", alias->name))
        return 0;
    if (!weechat_infolist_new_var_string (ptr_item, "command", alias->command))
        return 0;
    if (!weechat_infolist_new_var_string (ptr_item, "completion", alias->completion))
        return 0;
    if (!weechat_infolist_new_var_integer (ptr_item, "running", alias->running))
        return 0;
    
    return 1;
}

/*
 * weechat_plugin_init: initialize alias plugin
 */

int
weechat_plugin_init (struct t_weechat_plugin *plugin, int argc, char *argv[])
{
    /* make C compiler happy */
    (void) argc;
    (void) argv;
    
    weechat_plugin = plugin;
    
    if (!alias_config_init ())
    {
        weechat_printf (NULL,
                        _("%s%s: error creating configuration file"),
                        weechat_prefix("error"), ALIAS_PLUGIN_NAME);
        return WEECHAT_RC_ERROR;
    }
    alias_config_read ();
    
    weechat_hook_command ("alias",
                          N_("create an alias for a command"),
                          N_("[[-completion completion] alias_name [command "
                             "[arguments]]]"),
                          N_("completion: completion for alias (optional, by "
                             "default completion is done with target "
                             "command)\n"
                             "alias_name: name of alias (can start or end "
                             "with \"*\" for alias listing)\n"
                             "   command: command name (many commands can be "
                             "separated by semicolons)\n"
                             " arguments: arguments for command\n\n"
                             "Without argument, this command lists all "
                             "defined alias.\n\n"
                             "Note: in command, special variables "
                             "are replaced:\n"
                             "        $n: argument 'n' (between 1 and 9)\n"
                             "       $-m: arguments from 1 to 'm'\n"
                             "       $n-: arguments from 'n' to last\n"
                             "      $n-m: arguments from 'n' to 'm'\n"
                             "        $*: all arguments\n"
                             "        $~: last argument\n"
                             "     $nick: current nick\n"
                             "  $channel: current channel\n"
                             "   $server: current server\n\n"
                             "Examples:\n"
                             "  alias /split to split window horizontally:\n"
                             "    /alias split /window splith\n"
                             "  alias /hello to say \"hello\" on all channels "
                             "but not on #weechat:\n"
                             "    /alias hello /allchan -exclude=#weechat msg "
                             "* hello\n"
                             "  alias /forcejoin to send IRC command "
                             "\"forcejoin\" with completion of /sajoin:\n"
                             "    /alias -completion %%sajoin /quote forcejoin"),
                          "%(alias)|-completion %(commands)",
                          &alias_command_cb, NULL);
    
    weechat_hook_command ("unalias", N_("remove aliases"),
                          N_("alias_name [alias_name...]"),
                          N_("alias_name: name of alias to remove"),
                          "%(alias)|%*",
                          &unalias_command_cb, NULL);
    
    weechat_hook_completion ("alias", N_("list of aliases"),
                             &alias_completion_cb, NULL);
    
    alias_info_init ();
    
    return WEECHAT_RC_OK;
}

/*
 * weechat_plugin_end: end alias plugin
 */

int
weechat_plugin_end (struct t_weechat_plugin *plugin)
{
    /* make C compiler happy */
    (void) plugin;
    
    alias_config_write ();
    alias_free_all ();
    weechat_config_free (alias_config_file);
    
    return WEECHAT_RC_OK;
}
