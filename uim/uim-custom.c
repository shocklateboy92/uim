/*

  Copyright (c) 2003,2004,2005 uim Project http://uim.freedesktop.org/

  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.
  3. Neither the name of authors nor the names of its contributors
     may be used to endorse or promote products derived from this software
     without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
  OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
  OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
  SUCH DAMAGE.

*/

/*
  static functions that have uim_custom_ prefix could be exported as API
  function if needed.  -- YamaKen 2004-12-30
*/

/*
  Don't insert NULL checks for free(3). free(3) accepts NULL as proper
  argument that causes no action.  -- YamaKen 2004-12-17
*/

#include "config.h"

#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "uim-scm.h"
#include "uim-compat-scm.h"
#include "uim-custom.h"
#include "context.h"
#include "uim-helper.h"


typedef void (*uim_custom_cb_update_cb_t)(void *ptr, const char *custom_sym);

static char *c_list_to_str(const void *const *list, char *(*mapper)(const void *elem), const char *sep);

static int uim_custom_type_eq(const char *custom_sym, const char *custom_type);
static int uim_custom_type(const char *custom_sym);
static int uim_custom_is_active(const char *custom_sym);
static char *uim_custom_get_str(const char *custom_sym, const char *proc);
static char *uim_custom_label(const char *custom_sym);
static char *uim_custom_desc(const char *custom_sym);

static struct uim_custom_choice *uim_custom_choice_get(const char *custom_sym, const char *choice_sym);
static char *extract_choice_symbol(const struct uim_custom_choice *custom_choice);
static char *choice_list_to_str(const struct uim_custom_choice *const *list, const char *sep);
static void uim_custom_choice_free(struct uim_custom_choice *custom_choice);
static struct uim_custom_choice **extract_choice_list(const char *list_repl, const char *custom_sym);
static struct uim_custom_choice **uim_custom_choice_item_list(const char *custom_sym);

static struct uim_custom_choice **uim_custom_olist_get(const char *custom_sym);
static struct uim_custom_choice **uim_custom_olist_item_list(const char *custom_sym);

static struct uim_custom_key **uim_custom_key_get(const char *custom_sym);
static void uim_custom_key_free(struct uim_custom_key *custom_key);
static char *extract_key_literal(const struct uim_custom_key *custom_key);
static char *key_list_to_str(const struct uim_custom_key *const *list, const char *sep);
void uim_custom_key_list_free(struct uim_custom_key **list);

static union uim_custom_value *uim_custom_value_internal(const char *custom_sym, const char *getter_proc);
static union uim_custom_value *uim_custom_value(const char *custom_sym);
static union uim_custom_value *uim_custom_default_value(const char *custom_sym);
static void uim_custom_value_free(int custom_type, union uim_custom_value *custom_value);
static uim_lisp uim_custom_range_elem(const char *custom_sym, const char *accessor_proc);
static union uim_custom_range *uim_custom_range_get(const char *custom_sym);
static void uim_custom_range_free(int custom_type, union uim_custom_range *custom_range);
static uim_lisp uim_custom_cb_update_cb_gate(uim_lisp cb, uim_lisp ptr, uim_lisp custom_sym);

static void helper_disconnect_cb(void);
static char *uim_conf_path(const char *subpath);
static char *custom_file_path(const char *group, pid_t pid);
static uim_bool prepare_dir(const char *dir);
static uim_bool uim_conf_prepare_dir(const char *subdir);
static uim_bool for_each_primary_groups(uim_bool (*func)(const char *));
static uim_bool uim_custom_load_group(const char *group);
static uim_bool uim_custom_save_group(const char *group);

static const char str_list_arg[] = "uim-custom-c-str-list-arg";
static const char custom_subdir[] = "customs";
static const char custom_msg_tmpl[] = "prop_update_custom\n%s\n%s\n";
static int helper_fd = -1;
static uim_lisp return_val;


static char *
c_list_to_str(const void *const *list, char *(*mapper)(const void *elem), const char *sep)
{
  size_t buf_size;
  char *buf, *bufp, *str;
  const void *const *elem;

  buf_size = sizeof('\0');
  for (elem = list; *elem; elem++) {
    if (elem != list)
      buf_size += strlen(sep);
    str = (*mapper)(*elem);
    buf_size += strlen(str);
    free(str);
  }
  buf = (char *)malloc(buf_size);

  for (bufp = buf, elem = list; *elem; elem++) {
    if (elem != list) {
      strcpy(bufp, sep);
      bufp += strlen(sep);
    }
    str = (*mapper)(*elem);
    strcpy(bufp, str);
    bufp += strlen(str);
    free(str);
  }

  return buf;
}

static int
uim_custom_type_eq(const char *custom_sym, const char *custom_type)
{
  UIM_EVAL_FSTRING2(NULL, "(eq? (custom-type '%s) '%s)",
		    custom_sym, custom_type);

  return uim_scm_c_bool(uim_scm_return_value());
}

static int
uim_custom_type(const char *custom_sym)
{
  if (uim_custom_type_eq(custom_sym, "boolean")) {
    return UCustom_Bool;
  } else if (uim_custom_type_eq(custom_sym, "integer")) {
    return UCustom_Int;
  } else if (uim_custom_type_eq(custom_sym, "string")) {
    return UCustom_Str;
  } else if (uim_custom_type_eq(custom_sym, "pathname")) {
    return UCustom_Pathname;
  } else if (uim_custom_type_eq(custom_sym, "choice")) {
    return UCustom_Choice;
  } else if (uim_custom_type_eq(custom_sym, "ordered-list")) {
    return UCustom_OrderedList;
  } else if (uim_custom_type_eq(custom_sym, "key")) {
    return UCustom_Key;
  } else {
    return UCustom_Bool;
  }
}

static int
uim_custom_is_active(const char *custom_sym)
{
  UIM_EVAL_FSTRING1(NULL, "(custom-active? '%s)", custom_sym);
  return_val = uim_scm_return_value();

  return uim_scm_c_bool(return_val);
}

static char *
uim_custom_get_str(const char *custom_sym, const char *proc)
{
  UIM_EVAL_FSTRING2(NULL, "(%s '%s)", proc, custom_sym);
  return_val = uim_scm_return_value();

  /*
    The arg must be assigned to return_val to be proteced from GC
    while evaluation
  */
  return uim_scm_c_str(return_val);
}

static char *
uim_custom_label(const char *custom_sym)
{
  return uim_custom_get_str(custom_sym, "custom-label");
}

static char *
uim_custom_desc(const char *custom_sym)
{
  return uim_custom_get_str(custom_sym, "custom-desc");
}

/* choice */
static struct uim_custom_choice *
uim_custom_choice_get(const char *custom_sym, const char *choice_sym)
{
  struct uim_custom_choice *c_choice;

  c_choice = uim_custom_choice_new(NULL, NULL, NULL);
  if (!c_choice)
    return NULL;

  c_choice->symbol = strdup(choice_sym);

  UIM_EVAL_FSTRING2(NULL, "(custom-choice-label '%s '%s)",
		    custom_sym, choice_sym);
  return_val = uim_scm_return_value();
  c_choice->label = uim_scm_c_str(return_val);

  UIM_EVAL_FSTRING2(NULL, "(custom-choice-desc '%s '%s)",
		    custom_sym, choice_sym);
  return_val = uim_scm_return_value();
  c_choice->desc = uim_scm_c_str(return_val);

  return c_choice;
}

/**
 * TODO
 */
struct uim_custom_choice *
uim_custom_choice_new(char *symbol, char *label, char *desc)
{
  struct uim_custom_choice *custom_choice;

  custom_choice = (struct uim_custom_choice *)malloc(sizeof(struct uim_custom_choice));
  if (!custom_choice)
    return NULL;

  custom_choice->symbol = symbol;
  custom_choice->label = label;
  custom_choice->desc = desc;

  return custom_choice;
}

static void
uim_custom_choice_free(struct uim_custom_choice *custom_choice)
{
  if (!custom_choice)
    return;

  free(custom_choice->symbol);
  free(custom_choice->label);
  free(custom_choice->desc);
  free(custom_choice);
}

static struct uim_custom_choice **
extract_choice_list(const char *list_repl, const char *custom_sym)
{
  char *choice_sym, **choice_sym_list, **p;
  struct uim_custom_choice *custom_choice, **custom_choice_list;

  choice_sym_list =
    (char **)uim_scm_c_list(list_repl, "symbol->string",
			    (uim_scm_c_list_conv_func)uim_scm_c_str);
  if (!choice_sym_list)
    return NULL;

  for (p = choice_sym_list; choice_sym = *p; p++) {
    custom_choice = uim_custom_choice_get(custom_sym, choice_sym);
    *p = (char *)custom_choice;  /* intentionally overwrite */
  }
  /* reuse the list structure */
  custom_choice_list = (struct uim_custom_choice **)choice_sym_list;

  return custom_choice_list;
}

static struct uim_custom_choice **
uim_custom_choice_item_list(const char *custom_sym)
{
  UIM_EVAL_FSTRING2(NULL, "(define %s (custom-range '%s))",
		    str_list_arg, custom_sym);
  return extract_choice_list(str_list_arg, custom_sym);
}

static char *
extract_choice_symbol(const struct uim_custom_choice *custom_choice)
{
  return strdup(custom_choice->symbol);
}

static char *
choice_list_to_str(const struct uim_custom_choice *const *list, const char *sep)
{
  return c_list_to_str((const void *const *)list,
		       (char *(*)(const void *))extract_choice_symbol, sep);
}

/**
 * TODO
 */
void
uim_custom_choice_list_free(struct uim_custom_choice **list)
{
  uim_scm_c_list_free((void **)list,
		      (uim_scm_c_list_free_func)uim_custom_choice_free);
}

/* ordered list */
static struct uim_custom_choice **
uim_custom_olist_get(const char *custom_sym)
{
  UIM_EVAL_FSTRING2(NULL, "(define %s (custom-value '%s))",
		    str_list_arg, custom_sym);
  return extract_choice_list(str_list_arg, custom_sym);
}

static struct uim_custom_choice **
uim_custom_olist_item_list(const char *custom_sym)
{
  return uim_custom_choice_item_list(custom_sym);
}

/* key */
static struct uim_custom_key **
uim_custom_key_get(const char *custom_sym)
{
  char **key_literal_list, **key_label_list, **key_desc_list;
  int *key_type_list, editor_type, list_len, i;
  struct uim_custom_key *custom_key, **custom_key_list;

  UIM_EVAL_FSTRING3(NULL, "(define %s (custom-expand-key-references '%s (custom-range '%s))",
		    str_list_arg, custom_sym, custom_sym);
  key_literal_list =
    (char **)uim_scm_c_list(str_list_arg,
			    "(lambda (key) (if (symbol? key) symbol->string key))",
			    (uim_scm_c_list_conv_func)uim_scm_c_str);
  key_type_list =
    (int *)uim_scm_c_list(str_list_arg,
			  "(lambda (key) (if (symbol? key) 1 0))",
			  (uim_scm_c_list_conv_func)uim_scm_c_int);
  key_label_list =
    (char **)uim_scm_c_list(str_list_arg,
			    "(lambda (key) (if (symbol? key) (custom-label key) #f))",
			    (uim_scm_c_list_conv_func)uim_scm_c_str);
  key_desc_list =
    (char **)uim_scm_c_list(str_list_arg,
			    "(lambda (key) (if (symbol? key) (custom-desc key) #f))",
			    (uim_scm_c_list_conv_func)uim_scm_c_str);
  if (!key_type_list || !key_literal_list || !key_label_list || !key_desc_list)
  {
    free(key_type_list);
    uim_custom_symbol_list_free(key_literal_list);
    uim_custom_symbol_list_free(key_label_list);
    uim_custom_symbol_list_free(key_desc_list);
    return NULL;
  }

  UIM_EVAL_FSTRING1(NULL, "(custom-key-advanced-editor? '%s)", custom_sym);
  return_val = uim_scm_return_value();
  editor_type = uim_scm_c_bool(return_val) ? UCustomKeyEditor_Advanced : UCustomKeyEditor_Basic;

  UIM_EVAL_FSTRING1(NULL, "(length %s)", str_list_arg);
  return_val = uim_scm_return_value();
  list_len = uim_scm_c_int(return_val);

  for (i = 0; i < list_len; i++) {
    char *literal, *label, *desc;
    int type;
    type = (key_type_list[i] == 1) ? UCustomKey_Reference : UCustomKey_Regular;
    literal = key_literal_list[i];
    label = key_label_list[i];
    desc = key_desc_list[i];
    custom_key = uim_custom_key_new(type, editor_type, literal, label, desc);
    key_literal_list[i] = (char *)custom_key;  /* intentionally overwrite */
  }
  /* reuse the list structure */
  custom_key_list = (struct uim_custom_key **)key_literal_list;

  /* ownership of elements had been transferred to custom_key_list */
  free(key_type_list);
  free(key_label_list);
  free(key_desc_list);

  return custom_key_list;
}

/**
 * TODO
 */
struct uim_custom_key *
uim_custom_key_new(int type, int editor_type,
		   char *literal, char *label, char *desc)
{
  struct uim_custom_key *custom_key;

  custom_key = (struct uim_custom_key *)malloc(sizeof(struct uim_custom_key));
  if (!custom_key)
    return NULL;

  custom_key->type = type;
  custom_key->editor_type = editor_type;
  custom_key->literal = literal;
  custom_key->label = label;
  custom_key->desc = desc;

  return custom_key;
}

static void
uim_custom_key_free(struct uim_custom_key *custom_key)
{
  if (!custom_key)
    return;

  free(custom_key->literal);
  free(custom_key->label);
  free(custom_key->desc);
  free(custom_key);
}

static char *
extract_key_literal(const struct uim_custom_key *custom_key)
{
  char *literal;

  switch (custom_key->type) {
  case UCustomKey_Regular:
    UIM_EVAL_FSTRING1(NULL, "\"%s\"", custom_key->literal);
    literal = uim_scm_c_str(uim_scm_return_value());
    break;
  case UCustomKey_Reference:
    literal = strdup(custom_key->literal);
    break;
  default:
    literal = strdup("\"\"");
  }

  return literal;
}

static char *
key_list_to_str(const struct uim_custom_key *const *list, const char *sep)
{
  return c_list_to_str((const void *const *)list,
		       (char *(*)(const void *))extract_key_literal, sep);
}

/**
 * TODO
 */
void
uim_custom_key_list_free(struct uim_custom_key **list)
{
  uim_scm_c_list_free((void **)list,
		      (uim_scm_c_list_free_func)uim_custom_key_free);
}

static union uim_custom_value *
uim_custom_value_internal(const char *custom_sym, const char *getter_proc)
{
  int type;
  union uim_custom_value *value;
  char *custom_value_symbol;

  if (!custom_sym || !getter_proc)
    return NULL;

  value = (union uim_custom_value *)malloc(sizeof(union uim_custom_value));
  if (!value)
    return NULL;

  type = uim_custom_type(custom_sym);
  UIM_EVAL_FSTRING2(NULL, "(%s '%s)", getter_proc, custom_sym);
  return_val = uim_scm_return_value();
  switch (type) {
  case UCustom_Bool:
    value->as_bool = uim_scm_c_bool(return_val);
    break;
  case UCustom_Int:
    value->as_int = uim_scm_c_int(return_val);
    break;
  case UCustom_Str:
    value->as_str = uim_scm_c_str(return_val);
    break;
  case UCustom_Pathname:
    value->as_pathname = uim_scm_c_str(return_val);
    break;
  case UCustom_Choice:
    custom_value_symbol = uim_scm_c_symbol(return_val);
    value->as_choice = uim_custom_choice_get(custom_sym, custom_value_symbol);
    free(custom_value_symbol);
    break;
  case UCustom_OrderedList:
    value->as_olist = uim_custom_olist_get(custom_sym);
    break;
  case UCustom_Key:
    value->as_key = uim_custom_key_get(custom_sym);
    break;
  default:
    value = NULL;
  }

  return value;
}

static union uim_custom_value *
uim_custom_value(const char *custom_sym)
{
  return uim_custom_value_internal(custom_sym, "custom-value");
}

static union uim_custom_value *
uim_custom_default_value(const char *custom_sym)
{
  return uim_custom_value_internal(custom_sym, "custom-default-value");
}

static void
uim_custom_value_free(int custom_type, union uim_custom_value *custom_value)
{
  if (!custom_value)
    return;

  switch (custom_type) {
  case UCustom_Str:
    free(custom_value->as_str);
    break;
  case UCustom_Pathname:
    free(custom_value->as_pathname);
    break;
  case UCustom_Choice:
    uim_custom_choice_free(custom_value->as_choice);
    break;
  case UCustom_OrderedList:
    uim_custom_choice_list_free(custom_value->as_olist);
    break;
  case UCustom_Key:
    uim_custom_key_list_free(custom_value->as_key);
    break;
  }
  free(custom_value);
}

/*
  The arg must be assigned to return_val by caller to be proteced
  from GC while subsequent evaluation
*/
static uim_lisp
uim_custom_range_elem(const char *custom_sym, const char *accessor_proc)
{
  UIM_EVAL_FSTRING2(NULL, "(%s (custom-range '%s))",
		    accessor_proc, custom_sym);
  
  return uim_scm_return_value();
}

static union uim_custom_range *
uim_custom_range_get(const char *custom_sym)
{
  int type;
  union uim_custom_range *range;

  range = (union uim_custom_range *)malloc(sizeof(union uim_custom_range));
  if (!range)
    return NULL;

  type = uim_custom_type(custom_sym);
  switch (type) {
  case UCustom_Int:
    return_val = uim_custom_range_elem(custom_sym, "car");
    range->as_int.min = uim_scm_c_int(return_val);
    return_val = uim_custom_range_elem(custom_sym, "cadr");
    range->as_int.max = uim_scm_c_int(return_val);
    break;
  case UCustom_Str:
    return_val = uim_custom_range_elem(custom_sym, "car");
    range->as_str.regex = uim_scm_c_str(return_val);
    break;
  case UCustom_Choice:
    range->as_choice.valid_items = uim_custom_choice_item_list(custom_sym);
    break;
  case UCustom_OrderedList:
    range->as_olist.valid_items = uim_custom_olist_item_list(custom_sym);
    break;
  }

  return range;
}

static void
uim_custom_range_free(int custom_type, union uim_custom_range *custom_range)
{
  if (!custom_range)
    return;

  switch (custom_type) {
  case UCustom_Str:
    free(custom_range->as_str.regex);
    break;
  case UCustom_Choice:
    uim_custom_choice_list_free(custom_range->as_choice.valid_items);
    break;
  case UCustom_OrderedList:
    uim_custom_choice_list_free(custom_range->as_olist.valid_items);
    break;
  }
  free(custom_range);
}

static void
helper_disconnect_cb(void)
{
  helper_fd = -1;
}

/**
 * Initializes custom API. This function must be called before uim_custom_*()
 * functions are called. uim_init() must be called before this function.
 *
 * @see uim_init()
 * @retval UIM_TRUE succeeded
 * @retval UIM_FALSE failed
 */
uim_bool
uim_custom_init(void)
{
  return_val = uim_scm_f();

  uim_scm_gc_protect(&return_val);

  uim_scm_init_subr_3("custom-update-cb-gate", uim_custom_cb_update_cb_gate);

  uim_scm_require_file("custom.scm");

  return UIM_TRUE;
}

/**
 * Finalizes custom API. This function must be called before uim_quit().
 *
 * @see uim_quit()
 * @retval UIM_TRUE succeeded
 * @retval UIM_FALSE failed
 */
uim_bool
uim_custom_quit(void)
{
  /* TODO */
  uim_custom_cb_remove(NULL);

  return UIM_TRUE;
}

static char *
uim_conf_path(const char *subpath)
{
  char *dir;

  UIM_EVAL_STRING(NULL, "(string-append (getenv \"HOME\") \"/.uim.d\")");
  dir = uim_scm_c_str(uim_scm_return_value());
  if (subpath) {
    UIM_EVAL_FSTRING2(NULL, "\"%s/%s\"", dir, subpath);
    free(dir);
    dir = uim_scm_c_str(uim_scm_return_value());
  }

  return dir;
}

static char *
custom_file_path(const char *group, pid_t pid)
{
  char *custom_dir, *file_path;

  custom_dir = uim_conf_path(custom_subdir);
  if (pid) {
    UIM_EVAL_FSTRING3(NULL, "\"%s/.custom-%s.scm.%d\"", custom_dir, group, pid);
  } else {
    UIM_EVAL_FSTRING2(NULL, "\"%s/custom-%s.scm\"", custom_dir, group);
  }
  file_path = uim_scm_c_str(uim_scm_return_value());
  free(custom_dir);

  return file_path;
}

static uim_bool
prepare_dir(const char *dir)
{
  struct stat st;

  if (stat(dir, &st) < 0) {
    return (mkdir(dir, 0700) < 0) ? UIM_FALSE : UIM_TRUE;
  } else {
    mode_t mode = S_IFDIR | S_IRUSR | S_IWUSR | S_IXUSR;

    return ((st.st_mode & mode) == mode) ? UIM_TRUE : UIM_FALSE;
  }
}

static uim_bool
uim_conf_prepare_dir(const char *subdir)
{
  uim_bool succeeded;
  char *dir;

  dir = uim_conf_path(NULL);
  succeeded = prepare_dir(dir);
  free(dir);
  if (!succeeded)
    return UIM_FALSE;

  if (subdir) {
    dir = uim_conf_path(subdir);
    succeeded = prepare_dir(dir);
    free(dir);
    if (!succeeded)
      return UIM_FALSE;
  }

  return UIM_TRUE;
}

static uim_bool
for_each_primary_groups(uim_bool (*func)(const char *))
{
  uim_bool succeeded = UIM_TRUE;
  char **primary_groups, **grp;

  primary_groups = uim_custom_primary_groups();
  for (grp = primary_groups; *grp; grp++) {
    succeeded = (*func)(*grp) && succeeded;
  }
  uim_custom_symbol_list_free(primary_groups);

  return succeeded;
}

static uim_bool
uim_custom_load_group(const char *group)
{
  char *file_path;
  uim_bool succeeded;

  file_path = custom_file_path(group, 0);
  succeeded = uim_scm_load_file(file_path);
  free(file_path);

  return succeeded;
}

/**
 * Loads per-user custom variable configurations. This function loads per-user
 * custom variable values from ~/.uim.d/customs/custom-*.scm previously saved
 * by uim_custom_save().
 *
 * @see uim_custom_save()
 * @retval UIM_TRUE succeeded
 * @retval UIM_FALSE failed
 */
uim_bool
uim_custom_load(void)
{
  return for_each_primary_groups(uim_custom_load_group);
}

static uim_bool
uim_custom_save_group(const char *group)
{
  uim_bool succeeded = UIM_FALSE;
  char **custom_syms, **sym;
  char *def_literal;
  pid_t pid;
  char *tmp_file_path, *file_path;
  FILE *file;

  if (!uim_conf_prepare_dir(custom_subdir))
    return UIM_FALSE;

  /*
    to avoid write conflict and broken by accident, we write customs
    to temporary file first
  */
  pid = getpid();
  tmp_file_path = custom_file_path(group, pid);
  file = fopen(tmp_file_path, "w");
  if (!file)
    goto error;

  custom_syms = uim_custom_collect_by_group(group);
  if (!custom_syms)
    goto error;

  for (sym = custom_syms; *sym; sym++) {
    def_literal = uim_custom_definition_as_literal(*sym);
    if (def_literal) {
      fprintf(file, def_literal);
      fprintf(file, "\n");
      free(def_literal);
    }
  }
  uim_custom_symbol_list_free(custom_syms);

  if (fclose(file) < 0)
    goto error;
  /* rename prepared temporary file to proper name */
  file_path = custom_file_path(group, 0);
  succeeded = (rename(tmp_file_path, file_path) == 0);
  free(file_path);
 error:
  free(tmp_file_path);

  return succeeded;
}

/**
 * Saves per-user custom variable configurations. This function saves current
 * custom variable values into ~/.uim.d/customs/custom-*.scm. The directory
 * will be made if not exist. The saved values will be implicitly loaded at
 * uim_init() or can explicitly be loaded by uim_custom_load().
 *
 * @see uim_init()
 * @see uim_custom_load()
 * @retval UIM_TRUE succeeded
 * @retval UIM_FALSE failed
 */
uim_bool
uim_custom_save(void)
{
  return for_each_primary_groups(uim_custom_save_group);
}

/**
 * Broadcasts custom variable configurations to other uim-enabled application
 * processes via uim-helper-server. This function broadcasts current custom
 * variable values to other uim-enabled application processes via
 * uim-helper-server. The received processes updates custom variables
 * dynamically. This enables dynamic re-configuration of input methods.
 *
 * @retval UIM_TRUE succeeded
 * @retval UIM_FALSE failed
 */
uim_bool
uim_custom_broadcast(void)
{
  char **custom_syms, **sym;
  char *value, *msg;
  size_t msg_size;

  if (helper_fd < 0) {
    helper_fd = uim_helper_init_client_fd(helper_disconnect_cb);
  }

  custom_syms = uim_custom_collect_by_group(NULL);
  for (sym = custom_syms; *sym; sym++) {
    value = uim_custom_value_as_literal(*sym);
    if (value) {
      msg_size = sizeof(custom_msg_tmpl) + strlen(*sym) + strlen(value);
      msg = (char *)malloc(msg_size);
      if (!msg) {
	free(value);
	uim_custom_symbol_list_free(custom_syms);
	return UIM_FALSE;
      }
      sprintf(msg, custom_msg_tmpl, *sym, value);
      uim_helper_send_message(helper_fd, msg);
      free(msg);
      free(value);
    }
  }
  uim_custom_symbol_list_free(custom_syms);

  if (helper_fd != -1) {
    uim_helper_close_client_fd(helper_fd);
  }

  return UIM_TRUE;
}

/**
 * Returns attributes and current value of a custom variable. Returned value
 * must be freed by uim_custom_free().
 *
 * @see uim_custom_free()
 * @return custom variable attributes and current value
 * @param custom_sym custom variable name
 */
struct uim_custom *
uim_custom_get(const char *custom_sym)
{
  struct uim_custom *custom;

  if (!custom_sym)
    return UIM_FALSE;

  custom = (struct uim_custom *)malloc(sizeof(struct uim_custom));
  if (!custom)
    return UIM_FALSE;

  custom->type = uim_custom_type(custom_sym);
  custom->is_active = uim_custom_is_active(custom_sym);
  custom->symbol = strdup(custom_sym);
  custom->label = uim_custom_label(custom_sym);
  custom->desc = uim_custom_desc(custom_sym);
  custom->value = uim_custom_value(custom_sym);
  custom->default_value = uim_custom_default_value(custom_sym);
  custom->range = uim_custom_range_get(custom_sym);

  return custom;
}

/**
 * Updates value of a custom variable. This function tries that an update of
 * the custom variable specified by symbol and value of contained in @a
 * custom. Update failes when passed value is invalid for the custom
 * variable. Previous value is kept in real custom variable when the
 * failure. @a custom should be created by uim_custom_get() and then user of
 * uim-custom API can modify value of the @custom before passing to this
 * function.
 *
 * @see uim_custom_get()
 * @param custom custom variable symbol and value
 * @retval UIM_TRUE succeeded
 * @retval UIM_FALSE failed
 */
uim_bool
uim_custom_set(const struct uim_custom *custom)
{
  if (!custom)
    return UIM_FALSE;

  switch (custom->type) {
  case UCustom_Bool:
    UIM_EVAL_FSTRING2(NULL, "(custom-set-value! '%s #%s)",
		      custom->symbol, (custom->value->as_bool) ? "t" : "f");
    break;
  case UCustom_Int:
    UIM_EVAL_FSTRING2(NULL, "(custom-set-value! '%s %d)",
		      custom->symbol, custom->value->as_int);
    break;
  case UCustom_Str:
    UIM_EVAL_FSTRING2(NULL, "(custom-set-value! '%s \"%s\")",
		      custom->symbol, custom->value->as_str);
    break;
  case UCustom_Pathname:
    UIM_EVAL_FSTRING2(NULL, "(custom-set-value! '%s \"%s\")",
		      custom->symbol, custom->value->as_pathname);
    break;
  case UCustom_Choice:
    UIM_EVAL_FSTRING2(NULL, "(custom-set-value! '%s '%s)",
		      custom->symbol, custom->value->as_choice->symbol);
    break;
  case UCustom_OrderedList:
    {
      char *val;
      val = choice_list_to_str((const struct uim_custom_choice *const *)custom->value->as_olist, " ");
      UIM_EVAL_FSTRING2(NULL, "(custom-set-value! '%s '(%s))", custom->symbol, val);
      free(val);
    }
    break;
  case UCustom_Key:
    {
      char *val;
      val = key_list_to_str((const struct uim_custom_key *const *)custom->value->as_key, " ");
      UIM_EVAL_FSTRING2(NULL, "(custom-set-value! '%s '(%s))", custom->symbol, val);
      free(val);
    }
    break;
  default:
    return UIM_FALSE;
  }
  return uim_scm_c_bool(uim_scm_return_value());
}

/**
 * Frees pre-allocated C representation of a custom variable. All C
 * representation of a custom variable allocated by uim_custom_get() must be
 * freed by this function.
 *
 * @see uim_custom_get()
 * @param custom C representation of a custom variable
 */
void
uim_custom_free(struct uim_custom *custom)
{
  if (!custom)
    return;

  free(custom->symbol);
  free(custom->label);
  free(custom->desc);
  uim_custom_value_free(custom->type, custom->value);
  uim_custom_value_free(custom->type, custom->default_value);
  uim_custom_range_free(custom->type, custom->range);
  free(custom);
}

/**
 * Returns Scheme literal of a custom variable value. Returned string must be
 * free() by caller.
 *
 * @return the literal
 * @param custom_sym custom variable name
 */
char *
uim_custom_value_as_literal(const char *custom_sym)
{
  return uim_custom_get_str(custom_sym, "custom-value-as-literal");
}

/**
 * Returns Scheme literal of a custom variable definition. Returned string
 * must be free() by caller.
 *
 * @return the literal
 * @param custom_sym custom variable name
 */
char *
uim_custom_definition_as_literal(const char *custom_sym)
{
  return uim_custom_get_str(custom_sym, "custom-definition-as-literal");
}

/**
 * Returns attributes of a custom group.and current value of a custom
 * variable. Returned value must be freed by uim_custom_group_free().
 *
 * @see uim_custom_group_free()
 * @return attributes of custom group
 * @param group_sym custom group name
 */
struct uim_custom_group *
uim_custom_group_get(const char *group_sym)
{
  struct uim_custom_group *custom_group;

  custom_group = (struct uim_custom_group *)malloc(sizeof(struct uim_custom_group));
  if (!custom_group)
    return NULL;

  custom_group->symbol = strdup(group_sym);
  custom_group->label = uim_custom_get_str(group_sym, "custom-group-label");
  custom_group->desc = uim_custom_get_str(group_sym, "custom-group-desc");

  return custom_group;
}

/**
 * Frees C representation of a custom group.
 *
 * @see uim_custom_group_get()
 * @param custom_group C representation of custom group
 */
void
uim_custom_group_free(struct uim_custom_group *custom_group)
{
  if (!custom_group)
    return;

  free(custom_group->symbol);
  free(custom_group->label);
  free(custom_group->desc);
  free(custom_group);
}

/**
 * Returns custom variable symbols that belongs to @a group_sym. The symbols
 * consist of NULL-terminated array of C string and must be freed by
 * uim_custom_symbol_list_free().
 *
 * @see uim_custom_symbol_list_free()
 * @return custom variable symbols
 * @param group_sym custom group name. NULL means 'any group'
 */
char **
uim_custom_collect_by_group(const char *group_sym)
{
  char **custom_list;

  UIM_EVAL_FSTRING2(NULL, "(define %s (custom-collect-by-group '%s))",
		    str_list_arg, (group_sym) ? group_sym : "#f");
  custom_list = uim_scm_c_str_list(str_list_arg, "symbol->string");

  return custom_list;
}

/**
 * Returns all existing custom group symbols. The symbols consist of
 * NULL-terminated array of C string and must be freed by
 * uim_custom_symbol_list_free().
 *
 * @see uim_custom_symbol_list_free()
 * @return custom variable symbols
 */
char **
uim_custom_groups(void)
{
  char **group_list;

  UIM_EVAL_FSTRING1(NULL, "(define %s (custom-list-groups))", str_list_arg);
  group_list = uim_scm_c_str_list(str_list_arg, "symbol->string");

  return group_list;
}

/**
 * Returns all existing primary custom group symbols. Subgroups are not
 * returned. The symbols consist of NULL-terminated array of C string and must
 * be freed by uim_custom_symbol_list_free().
 *
 * @see uim_custom_symbol_list_free()
 * @return custom variable symbols
 */
char **
uim_custom_primary_groups(void)
{
  char **group_list;

  UIM_EVAL_FSTRING1(NULL, "(define %s (custom-list-primary-groups))",
		    str_list_arg);
  group_list = uim_scm_c_str_list(str_list_arg, "symbol->string");

  return group_list;
}

/**
 * Returns subgroup symbols of @a group_sym. The symbols consist of
 * NULL-terminated array of C string and must be freed by
 * uim_custom_symbol_list_free().
 *
 * @see uim_custom_symbol_list_free()
 * @return custom subgroup symbols
 * @param group_sym custom group name
 */
char **
uim_custom_group_subgroups(const char *group_sym)
{
  char **group_list;

  UIM_EVAL_FSTRING2(NULL, "(define %s (custom-group-subgroups '%s))",
		    str_list_arg, group_sym);
  group_list = uim_scm_c_str_list(str_list_arg, "symbol->string");

  return group_list;
}

/**
 * Frees a symbol list allocated by uim_custom_*() functions. The term 'list'
 * does not mean linked list. Actually NULL-terminated array.
 *
 * @see uim_custom_collect_by_group()
 * @see uim_custom_groups()
 * @see uim_custom_primary_groups()
 * @see uim_custom_group_subgroups()
 * @param symbol_list pre-allocated symbol list
 */
void
uim_custom_symbol_list_free(char **symbol_list)
{
  uim_scm_c_list_free((void **)symbol_list, (uim_scm_c_list_free_func)free);
}

static uim_lisp
uim_custom_cb_update_cb_gate(uim_lisp cb, uim_lisp ptr, uim_lisp custom_sym)
{
  uim_custom_cb_update_cb_t update_cb;
  void *c_ptr;
  char *c_custom_sym;

  update_cb = (uim_custom_cb_update_cb_t)uim_scm_c_ptr(cb);
  c_ptr = uim_scm_c_ptr(ptr);
  c_custom_sym = uim_scm_c_symbol(custom_sym);
  (*update_cb)(c_ptr, c_custom_sym);

  return uim_scm_f();
}

/**
 * Set a callback function in a custom variable. The @a update_cb is called
 * back when the custom variable specified by @a custom_sym is
 * updated. Multiple callbacks for one custom variable is allowed.
 *
 * @retval UIM_TRUE succeeded
 * @retval UIM_FALSE failed
 * @param custom_sym custom variable name
 * @param ptr an opaque value passed back to client at callback
 * @param update_cb function pointer called back when the custom variable is
 *        updated
 */
uim_bool
uim_custom_cb_add(const char *custom_sym, void *ptr,
		  void (*update_cb)(void *ptr, const char *custom_sym))
{
  uim_bool succeeded;
  uim_lisp stack_start;
  uim_lisp form;

  uim_scm_gc_protect_stack(&stack_start);
  form = uim_scm_list5(uim_scm_make_symbol("custom-register-update-cb"),
		       uim_scm_make_symbol(custom_sym),
		       uim_scm_make_ptr(ptr),
		       uim_scm_make_symbol("custom-update-cb-gate"),
		       uim_scm_make_ptr((void *)update_cb));
  succeeded = uim_scm_c_bool(uim_scm_eval(form));
  uim_scm_gc_unprotect_stack(&stack_start);

  return succeeded;
}

/**
 * Remove the callback functions in a custom variable. All functions set for @a
 * custom_sym will be removed.
 *
 * @retval UIM_TRUE some functions are removed
 * @retval UIM_FALSE no functions are removed
 * @param custom_sym custom variable name. NULL instructs 'all callbacks'
 */
uim_bool
uim_custom_cb_remove(const char *custom_sym)
{
  uim_bool removed;

  UIM_EVAL_FSTRING1(NULL, "(custom-remove-hook '%s 'custom-update-hook)",
		    (custom_sym) ? custom_sym : "#f");
  removed = uim_scm_c_bool(uim_scm_return_value());

  return removed;
}
