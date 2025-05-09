
/* MODULE							HTGroup.c
**		GROUP FILE ROUTINES
**
**	Contains group file parser and routines to match IP
**	address templates and to find out group membership.
**
**
** AUTHORS:
**	AL	Ari Luotonen	luotonen@dxcern.cern.ch
**
** HISTORY:
**
**
** BUGS:
**
**
**
** GROUP DEFINITION GRAMMAR:
**
**	string = "sequence of alphanumeric characters"
**	user_name ::= string
**	group_name ::= string
**	group_ref ::= group_name
**	user_def ::= user_name | group_ref
**	user_def_list ::= user_def { ',' user_def }
**	user_part = user_def | '(' user_def_list ')'
**
**	templ = "sequence of alphanumeric characters and '*'s"
**	ip_number_mask ::= templ '.' templ '.' templ '.' templ
**	domain_name_mask ::= templ { '.' templ }
**	address ::= ip_number_mask | domain_name_mask
**	address_def ::= address
**	address_def_list ::= address_def { ',' address_def }
**	address_part = address_def | '(' address_def_list ')'
**
**	item ::= [user_part] ['@' address_part]
**	item_list ::= item { ',' item }
**	group_def ::= item_list
**	group_decl ::= group_name ':' group_def
**
*/

#include "../config.h"

#include <string.h>

#include "HTUtils.h"
#include "HTAAUtil.h"
#include "HTLex.h"              /* Lexical analysor     */
#include "HTGroup.h"            /* Implemented here     */

#ifndef DISABLE_TRACE
extern int www2Trace;
#endif

/*
** Group file parser
*/

typedef HTList UserDefList;
typedef HTList AddressDefList;

typedef struct {
    UserDefList *user_def_list;
    AddressDefList *address_def_list;
} Item;

typedef struct {
    char *name;
    GroupDef *translation;
} Ref;

PRIVATE void syntax_error ARGS3(FILE *, fp, char *, msg, LexItem, lex_item)
{
    char buffer[41];
    int cnt = 0;
    char ch;

    while ((ch = getc(fp)) != EOF && ch != '\n')
        if (cnt < 40)
            buffer[cnt++] = ch;
    buffer[cnt] = (char)0;

#ifndef DISABLE_TRACE
    if (www2Trace)
        fprintf(stderr, "%s %d before: '%s'\nHTGroup.c: %s (got %s)\n",
                "HTGroup.c: Syntax error in rule file at line", lex_line, buffer, msg, lex_verbose(lex_item));
#endif
    lex_line++;
}

PRIVATE AddressDefList *parse_address_part ARGS1(FILE *, fp)
{
    AddressDefList *address_def_list = NULL;
    LexItem lex_item;
    BOOL only_one = NO;

    lex_item = lex(fp);
    if (lex_item == LEX_ALPH_STR || lex_item == LEX_TMPL_STR)
        only_one = YES;
    else if (lex_item != LEX_OPEN_PAREN || ((lex_item = lex(fp)) != LEX_ALPH_STR && lex_item != LEX_TMPL_STR)) {
        syntax_error(fp, "Expecting a single address or '(' beginning list", lex_item);
        return NULL;
    }
    address_def_list = HTList_new();

    for (;;) {
        Ref *ref = (Ref *) malloc(sizeof(Ref));
        ref->name = NULL;
        ref->translation = NULL;
        StrAllocCopy(ref->name, lex_buffer);

        HTList_addObject(address_def_list, (void *)ref);

        if (only_one || (lex_item = lex(fp)) != LEX_ITEM_SEP)
            break;
        /*
         ** Here lex_item == LEX_ITEM_SEP; after item separator it
         ** is ok to have one or more newlines (LEX_REC_SEP) and
         ** they are ignored (continuation line).
         */
        do {
            lex_item = lex(fp);
        }
        while (lex_item == LEX_REC_SEP);

        if (lex_item != LEX_ALPH_STR && lex_item != LEX_TMPL_STR) {
            syntax_error(fp, "Expecting an address template", lex_item);
            HTList_delete(address_def_list);
            return NULL;
        }
    }

    if (!only_one && lex_item != LEX_CLOSE_PAREN) {
        HTList_delete(address_def_list);
        syntax_error(fp, "Expecting ')' closing address list", lex_item);
        return NULL;
    }
    return address_def_list;
}

PRIVATE UserDefList *parse_user_part ARGS1(FILE *, fp)
{
    UserDefList *user_def_list = NULL;
    LexItem lex_item;
    BOOL only_one = NO;

    lex_item = lex(fp);
    if (lex_item == LEX_ALPH_STR)
        only_one = YES;
    else if (lex_item != LEX_OPEN_PAREN || (lex_item = lex(fp)) != LEX_ALPH_STR) {
        syntax_error(fp, "Expecting a single name or '(' beginning list", lex_item);
        return NULL;
    }
    user_def_list = HTList_new();

    for (;;) {
        Ref *ref = (Ref *) malloc(sizeof(Ref));
        ref->name = NULL;
        ref->translation = NULL;
        StrAllocCopy(ref->name, lex_buffer);

        HTList_addObject(user_def_list, (void *)ref);

        if (only_one || (lex_item = lex(fp)) != LEX_ITEM_SEP)
            break;
        /*
         ** Here lex_item == LEX_ITEM_SEP; after item separator it
         ** is ok to have one or more newlines (LEX_REC_SEP) and
         ** they are ignored (continuation line).
         */
        do {
            lex_item = lex(fp);
        }
        while (lex_item == LEX_REC_SEP);

        if (lex_item != LEX_ALPH_STR) {
            syntax_error(fp, "Expecting user or group name", lex_item);
            HTList_delete(user_def_list);
            return NULL;
        }
    }

    if (!only_one && lex_item != LEX_CLOSE_PAREN) {
        HTList_delete(user_def_list);
        syntax_error(fp, "Expecting ')' closing user/group list", lex_item);
        return NULL;
    }
    return user_def_list;
}

PRIVATE Item *parse_item ARGS1(FILE *, fp)
{
    Item *item = NULL;
    UserDefList *user_def_list = NULL;
    AddressDefList *address_def_list = NULL;
    LexItem lex_item;

    lex_item = lex(fp);
    if (lex_item == LEX_ALPH_STR || lex_item == LEX_OPEN_PAREN) {
        unlex(lex_item);
        user_def_list = parse_user_part(fp);
        lex_item = lex(fp);
    }

    if (lex_item == LEX_AT_SIGN) {
        lex_item = lex(fp);
        if (lex_item == LEX_ALPH_STR || lex_item == LEX_TMPL_STR || lex_item == LEX_OPEN_PAREN) {
            unlex(lex_item);
            address_def_list = parse_address_part(fp);
        } else {
            if (user_def_list)
                HTList_delete(user_def_list);   /* @@@@ */
            syntax_error(fp, "Expected address part (single address or list)", lex_item);
            return NULL;
        }
    } else
        unlex(lex_item);

    if (!user_def_list && !address_def_list) {
        syntax_error(fp, "Empty item not allowed", lex_item);
        return NULL;
    }
    item = (Item *) malloc(sizeof(Item));
    item->user_def_list = user_def_list;
    item->address_def_list = address_def_list;
    return item;
}

PRIVATE ItemList *parse_item_list ARGS1(FILE *, fp)
{
    ItemList *item_list = HTList_new();
    Item *item;
    LexItem lex_item;

    for (;;) {
        if (!(item = parse_item(fp))) {
            HTList_delete(item_list);   /* @@@@ */
            return NULL;
        }
        HTList_addObject(item_list, (void *)item);
        lex_item = lex(fp);
        if (lex_item != LEX_ITEM_SEP) {
            unlex(lex_item);
            return item_list;
        }
        /*
         ** Here lex_item == LEX_ITEM_SEP; after item separator it
         ** is ok to have one or more newlines (LEX_REC_SEP) and
         ** they are ignored (continuation line).
         */
        do {
            lex_item = lex(fp);
        }
        while (lex_item == LEX_REC_SEP);
        unlex(lex_item);
    }
}

PUBLIC GroupDef *HTAA_parseGroupDef ARGS1(FILE *, fp)
{
    ItemList *item_list = NULL;
    GroupDef *group_def = NULL;
    LexItem lex_item;

    if (!(item_list = parse_item_list(fp))) {
        return NULL;
    }
    group_def = (GroupDef *) malloc(sizeof(GroupDef));
    group_def->group_name = NULL;
    group_def->item_list = item_list;

    if ((lex_item = lex(fp)) != LEX_REC_SEP) {
        syntax_error(fp, "Garbage after group definition", lex_item);
    }

    return group_def;
}

PRIVATE GroupDef *parse_group_decl ARGS1(FILE *, fp)
{
    char *group_name = NULL;
    GroupDef *group_def = NULL;
    LexItem lex_item;

    do {
        lex_item = lex(fp);
    }
    while (lex_item == LEX_REC_SEP);    /* Ignore empty lines */

    if (lex_item != LEX_ALPH_STR) {
        if (lex_item != LEX_EOF)
            syntax_error(fp, "Expecting group name", lex_item);
        return NULL;
    }
    StrAllocCopy(group_name, lex_buffer);

    if (LEX_FIELD_SEP != (lex_item = lex(fp))) {
        syntax_error(fp, "Expecting field separator", lex_item);
        free(group_name);
        return NULL;
    }

    if (!(group_def = HTAA_parseGroupDef(fp))) {
        free(group_name);
        return NULL;
    }
    group_def->group_name = group_name;

    return group_def;
}

/*
** Group manipulation routines
*/

PRIVATE GroupDef *find_group_def ARGS2(GroupDefList *, group_list, WWW_CONST char *, group_name)
{
    if (group_list && group_name) {
        GroupDefList *cur = group_list;
        GroupDef *group_def;

        while (NULL != (group_def = (GroupDef *) HTList_nextObject(cur))) {
            if (!strcmp(group_name, group_def->group_name)) {
                return group_def;
            }
        }
    }
    return NULL;
}

PUBLIC void HTAA_resolveGroupReferences ARGS2(GroupDef *, group_def, GroupDefList *, group_def_list)
{
    if (group_def && group_def->item_list && group_def_list) {
        ItemList *cur1 = group_def->item_list;
        Item *item;

        while (NULL != (item = (Item *) HTList_nextObject(cur1))) {
            UserDefList *cur2 = item->user_def_list;
            Ref *ref;

            while (NULL != (ref = (Ref *) HTList_nextObject(cur2)))
                ref->translation = find_group_def(group_def_list, ref->name);

            /* Does NOT translate address_def_list */
        }
    }
}

PRIVATE void add_group_def ARGS2(GroupDefList *, group_def_list, GroupDef *, group_def)
{
    HTAA_resolveGroupReferences(group_def, group_def_list);
    HTList_addObject(group_def_list, (void *)group_def);
}

PRIVATE GroupDefList *parse_group_file ARGS1(FILE *, fp)
{
    GroupDefList *group_def_list = HTList_new();
    GroupDef *group_def;

    while (NULL != (group_def = parse_group_decl(fp)))
        add_group_def(group_def_list, group_def);

    return group_def_list;
}

/*
** Trace functions
*/

PRIVATE void print_item ARGS1(Item *, item)
{
    if (!item)
        fprintf(stderr, "\tNULL-ITEM\n");
    else {
        UserDefList *cur1 = item->user_def_list;
        AddressDefList *cur2 = item->address_def_list;
        Ref *user_ref = (Ref *) HTList_nextObject(cur1);
        Ref *addr_ref = (Ref *) HTList_nextObject(cur2);

        if (user_ref) {
            fprintf(stderr, "\t[%s%s", user_ref->name, (user_ref->translation ? "*REF*" : ""));
            while (NULL != (user_ref = (Ref *) HTList_nextObject(cur1)))
                fprintf(stderr, "; %s%s", user_ref->name, (user_ref->translation ? "*REF*" : ""));
            fprintf(stderr, "] ");
        } else
            fprintf(stderr, "\tANYBODY ");

        if (addr_ref) {
            fprintf(stderr, "@ [%s", addr_ref->name);
            while (NULL != (addr_ref = (Ref *) HTList_nextObject(cur2)))
                fprintf(stderr, "; %s", addr_ref->name);
            fprintf(stderr, "]\n");
        } else
            fprintf(stderr, "@ ANYADDRESS\n");
    }
}

PRIVATE void print_item_list ARGS1(ItemList *, item_list)
{
    ItemList *cur = item_list;
    Item *item;

    if (!item_list)
        fprintf(stderr, "EMPTY");
    else
        while (NULL != (item = (Item *) HTList_nextObject(cur)))
            print_item(item);
}

PUBLIC void HTAA_printGroupDef ARGS1(GroupDef *, group_def)
{
    if (!group_def) {
        fprintf(stderr, "\nNULL RECORD\n");
        return;
    }

    fprintf(stderr, "\nGroup %s:\n", (group_def->group_name ? group_def->group_name : "NULL"));

    print_item_list(group_def->item_list);
    fprintf(stderr, "\n");
}

PRIVATE void print_group_def_list ARGS1(GroupDefList *, group_list)
{
    GroupDefList *cur = group_list;
    GroupDef *group_def;

    while (NULL != (group_def = (GroupDef *) HTList_nextObject(cur)))
        HTAA_printGroupDef(group_def);
}

/*
** IP address template matching
*/

/* PRIVATE						part_match()
**		MATCH ONE PART OF INET ADDRESS AGAIST
**		A PART OF MASK (inet address has 4 parts)
** ON ENTRY:
**	tcur	pointer to the beginning of template part.
**	icur	pointer to the beginning of actual inet
**		number part.
**
** ON EXIT:
**	returns	YES, if match.
*/
PRIVATE BOOL part_match ARGS2(WWW_CONST char *, tcur, WWW_CONST char *, icur)
{
    char required[4];
    char actual[4];
    WWW_CONST char *cur;
    int cnt;

    if (!tcur || !icur)
        return NO;

    cur = tcur;
    cnt = 0;
    while (cnt < 3 && *cur && *cur != '.')
        required[cnt++] = *(cur++);
    required[cnt] = (char)0;

    cur = icur;
    cnt = 0;
    while (cnt < 3 && *cur && *cur != '.')
        actual[cnt++] = *(cur++);
    actual[cnt] = (char)0;

#ifndef DISABLE_TRACE
    if (www2Trace) {
        BOOL status = HTAA_templateMatch(required, actual);
        fprintf(stderr, "part_match: req: '%s' act: '%s' match: %s\n", required, actual, (status ? "yes" : "no"));
        return status;
    }
#endif

    return HTAA_templateMatch(required, actual);
}

/* PRIVATE						ip_number_match()
**		MATCH INET NUMBER AGAINST AN INET NUMBER MASK
** ON ENTRY:
**	template	mask to match agaist, e.g. 128.141.*.*
**	inet_addr	actual inet address, e.g. 128.141.201.74
**
** ON EXIT:
**	returns		YES, if match;  NO, if not.
*/
PRIVATE BOOL ip_number_match ARGS2(WWW_CONST char *, template, WWW_CONST char *, inet_addr)
{
    WWW_CONST char *tcur = template;
    WWW_CONST char *icur = inet_addr;
    int cnt;

    for (cnt = 0; cnt < 4; cnt++) {
        if (!tcur || !icur || !part_match(tcur, icur))
            return NO;
        if (NULL != (tcur = strchr(tcur, '.')))
            tcur++;
        if (NULL != (icur = strchr(icur, '.')))
            icur++;
    }
    return YES;
}

/* PRIVATE						is_domain_mask()
**		DETERMINE IF A GIVEN MASK IS A
**		DOMAIN NAME MASK OR AN INET NUMBER MASK
** ON ENTRY:
**	mask	either a domain name mask,
**		e.g.
**			*.cern.ch
**
**		or an inet number mask,
**		e.g.
**			128.141.*.*
**
** ON EXIT:
**	returns	YES, if mask is a domain name mask.
**		NO, if it is an inet number mask.
*/
PRIVATE BOOL is_domain_mask ARGS1(WWW_CONST char *, mask)
{
    WWW_CONST char *cur = mask;

    if (!mask)
        return NO;

    while (*cur) {
        if (*cur != '.' && *cur != '*' && (*cur < '0' || *cur > '9'))
            return YES;         /* Even one non-digit makes it a domain name mask */
        cur++;
    }
    return NO;                  /* All digits and dots, so it is an inet number mask */
}

/* PRIVATE							ip_mask_match()
**		MATCH AN IP NUMBER MASK OR IP NAME MASK
**		AGAINST ACTUAL IP NUMBER OR IP NAME
**		
** ON ENTRY:
**	mask		mask. Mask may be either an inet number
**			mask or a domain name mask,
**			e.g.
**				128.141.*.*
**			or
**				*.cern.ch
**
**	ip_number	IP number of connecting host.
**	ip_name		IP name of the connecting host.
**
** ON EXIT:
**	returns		YES, if hostname/internet number
**			matches the mask.
**			NO, if no match (no fire).
*/
PRIVATE BOOL ip_mask_match ARGS3(WWW_CONST char *, mask, WWW_CONST char *, ip_number, WWW_CONST char *, ip_name)
{
    if (mask && (ip_number || ip_name)) {
        if (is_domain_mask(mask)) {
            if (HTAA_templateMatch(mask, ip_name))
                return YES;
        } else {
            if (ip_number_match(mask, ip_number))
                return YES;
        }
    }
    return NO;
}

PRIVATE BOOL ip_in_def_list ARGS3(AddressDefList *, address_def_list, char *, ip_number, char *, ip_name)
{
    if (address_def_list && (ip_number || ip_name)) {
        AddressDefList *cur = address_def_list;
        Ref *ref;

        while (NULL != (ref = (Ref *) HTList_nextObject(cur))) {
            /* Value of ref->translation is ignored, i.e. */
            /* no recursion for ip address tamplates.     */
            if (ip_mask_match(ref->name, ip_number, ip_name))
                return YES;
        }
    }
    return NO;
}

/*
** Group file cached reading
*/

typedef struct {
    char *group_filename;
    GroupDefList *group_list;
} GroupCache;

typedef HTList GroupCacheList;

PRIVATE GroupCacheList *group_cache_list = NULL;

PUBLIC GroupDefList *HTAA_readGroupFile ARGS1(WWW_CONST char *, filename)
{
    FILE *fp;
    GroupCache *group_cache;

    if (!group_cache_list)
        group_cache_list = HTList_new();
    else {
        GroupCacheList *cur = group_cache_list;

        while (NULL != (group_cache = (GroupCache *) HTList_nextObject(cur))) {
            if (!strcmp(filename, group_cache->group_filename)) {
#ifndef DISABLE_TRACE
                if (www2Trace)
                    fprintf(stderr, "%s '%s' %s\n",
                            "HTAA_readGroupFile: group file", filename, "already found in cache");
#endif
                return group_cache->group_list;
            }                   /* if cache match */
        }                       /* while cached files remain */
    }                           /* cache exists */

#ifndef DISABLE_TRACE
    if (www2Trace)
        fprintf(stderr, "HTAA_readGroupFile: reading group file `%s'\n", filename);
#endif

    if (!(fp = fopen(filename, "r"))) {
#ifndef DISABLE_TRACE
        if (www2Trace)
            fprintf(stderr, "%s '%s'\n", "HTAA_readGroupFile: unable to open group file", filename);
#endif
        return NULL;
    }

    if (!(group_cache = (GroupCache *) malloc(sizeof(GroupCache))))
        outofmem(__FILE__, "HTAA_readGroupFile");

    group_cache->group_filename = NULL;
    StrAllocCopy(group_cache->group_filename, filename);
    group_cache->group_list = parse_group_file(fp);
    HTList_addObject(group_cache_list, (void *)group_cache);
    fclose(fp);

#ifndef DISABLE_TRACE
    if (www2Trace) {
        fprintf(stderr, "Read group file '%s', results follow:\n", filename);
        print_group_def_list(group_cache->group_list);
    }
#endif

    return group_cache->group_list;
}

/* PUBLIC					HTAA_userAndInetInGroup()
**		CHECK IF USER BELONGS TO TO A GIVEN GROUP
**		AND THAT THE CONNECTION COMES FROM AN
**		ADDRESS THAT IS ALLOWED BY THAT GROUP
** ON ENTRY:
**	group		the group definition structure.
**	username	connecting user.
**	ip_number	browser host IP number, optional.
**	ip_name		browser host IP name, optional.
**			However, one of ip_number or ip_name
**			must be given.
** ON EXIT:
**	returns		HTAA_IP_MASK, if IP address mask was
**			reason for failing.
**			HTAA_NOT_MEMBER, if user does not belong
**			to the group.
**			HTAA_OK if both IP address and user are ok.
*/
PUBLIC HTAAFailReasonType HTAA_userAndInetInGroup ARGS4(GroupDef *, group,
                                                        char *, username, char *, ip_number, char *, ip_name)
{
    HTAAFailReasonType reason = HTAA_NOT_MEMBER;

    if (group && username) {
        ItemList *cur1 = group->item_list;
        Item *item;

        while (NULL != (item = (Item *) HTList_nextObject(cur1))) {
            if (!item->address_def_list ||  /* Any address allowed */
                ip_in_def_list(item->address_def_list, ip_number, ip_name)) {

                if (!item->user_def_list)   /* Any user allowed */
                    return HTAA_OK;
                else {
                    UserDefList *cur2 = item->user_def_list;
                    Ref *ref;

                    while (NULL != (ref = (Ref *) HTList_nextObject(cur2))) {

                        if (ref->translation) { /* Group, check recursively */
                            reason = HTAA_userAndInetInGroup(ref->translation, username, ip_number, ip_name);
                            if (reason == HTAA_OK)
                                return HTAA_OK;
                        } else {    /* Username, check directly */
                            if (username && *username && 0 == strcmp(ref->name, username))
                                return HTAA_OK;
                        }
                    }           /* Every user/group name in this group */
                }               /* search for username */
            }                   /* IP address ok */
            else {
                return HTAA_IP_MASK;
            }
        }                       /* while items in group */
    }                           /* valid parameters */

    return reason;              /* No match, or invalid parameters */
}

PUBLIC void GroupDef_delete ARGS1(GroupDef *, group_def)
{
    if (group_def) {
        FREE(group_def->group_name);
        if (group_def->item_list)
            HTList_delete(group_def->item_list);    /* @@@@ */
        free(group_def);
    }
}
