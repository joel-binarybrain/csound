 /*
    csound_orc_expressions.c:

    Copyright (C) 2006
    Steven Yi

    This file is part of Csound.

    The Csound Library is free software; you can redistribute it
    and/or modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    Csound is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with Csound; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
    02111-1307 USA
*/

#include "csoundCore.h"
#include "csound_orc.h"

extern char argtyp2(char *);
extern void print_tree(CSOUND *, char *, TREE *);
extern void handle_polymorphic_opcode(CSOUND*, TREE *);
extern void handle_optional_args(CSOUND *, TREE *);
extern ORCTOKEN *make_token(CSOUND *, char *);
extern ORCTOKEN *make_label(CSOUND *, char *);
extern OENTRIES* find_opcode2(CSOUND *, char*);
extern char resolve_opcode_get_outarg(CSOUND* , OENTRIES* , char*);


TREE* create_boolean_expression(CSOUND*, TREE*, int, int);
TREE * create_expression(CSOUND *, TREE *, int, int);

static int genlabs = 300;

char *create_out_arg(CSOUND *csound, char outype)
{
    char* s = (char *)csound->Malloc(csound, 16);
    switch(outype) {
    case 'a': sprintf(s, "#a%d", csound->acount++); break;
    case 'K':
    case 'k': sprintf(s, "#k%d", csound->kcount++); break;
    case 'B': sprintf(s, "#B%d", csound->Bcount++); break;
    case 'b': sprintf(s, "#b%d", csound->bcount++); break;
    case 't': sprintf(s, "#t%d", csound->tcount++); break;
    case 'S': sprintf(s, "#S%d", csound->tcount++); break;            
    default:  sprintf(s, "#i%d", csound->icount++); break;
    }
    return s;
}

/**
 * Handles expression opcode type, appending to passed in opname
 * returns outarg type
 */
char *set_expression_type(CSOUND *csound, char * op, char arg1, char arg2)
{
    char outype, *s;
    OENTRIES* oentries;

    oentries = find_opcode2(csound, op);
    char args[3] = { arg1, arg2, '\0' };
    
    outype = resolve_opcode_get_outarg(csound, oentries, args);
    
    s = create_out_arg(csound, outype);

    if (UNLIKELY(PARSER_DEBUG))
      csound->Message(csound, "SET_EXPRESSION_TYPE: %s : %s\n", op, s);

    return s;
}

char * get_boolean_arg(CSOUND *csound, int type)
{
    char* s = (char *)csound->Malloc(csound, 8);
    sprintf(s, "#%c%d", type?'B':'b',csound->Bcount++);
    
    return s;
}

int get_expression_ans_type(char * ans)
{
//    char * t = ans;
//    t++;
//
//    switch(*t) {
//    case 'a':
//      return T_IDENT_A;
//    case 'k':
//      return T_IDENT_K;
//    case 'B':
//      return T_IDENT_B;
//    case 'b':
//      return T_IDENT_b;
//    default:
//      return T_IDENT_I;
//    }
    return T_IDENT;
}

TREE *create_empty_token(CSOUND *csound)
{
    TREE *ans;
    ans = (TREE*)mmalloc(csound, sizeof(TREE));
    if (UNLIKELY(ans==NULL)) {
      /* fprintf(stderr, "Out of memory\n"); */
      exit(1);
    }
    ans->type = -1;
    ans->left = NULL;
    ans->right = NULL;
    ans->next = NULL;
    ans->len = 0;
    ans->rate = -1;
    ans->line = 0;
    ans->locn  = 0;
    ans->value = NULL;
    return ans;
}

TREE *create_minus_token(CSOUND *csound)
{
    TREE *ans;
    ans = (TREE*)mmalloc(csound, sizeof(TREE));
    if (UNLIKELY(ans==NULL)) {
      /* fprintf(stderr, "Out of memory\n"); */
      exit(1);
    }
    ans->type = INTEGER_TOKEN;
    ans->left = NULL;
    ans->right = NULL;
    ans->next = NULL;
    ans->len = 0;
    ans->rate = -1;
    ans->value = make_int(csound, "-1");
    return ans;
}

TREE * create_opcode_token(CSOUND *csound, char* op)
{
    TREE *ans = create_empty_token(csound);

    ans->type = T_OPCODE;
    ans->value = make_token(csound, op);
    ans->value->type = T_OPCODE;

    return ans;
}

TREE * create_ans_token(CSOUND *csound, char* var)
{
    TREE *ans = create_empty_token(csound);

    ans->type = get_expression_ans_type(var);
    ans->value = make_token(csound, var);
    ans->value->type = ans->type;

    return ans;

}

TREE * create_goto_token(CSOUND *csound, char * booleanVar,
                         TREE * gotoNode, int type)
{
/*     TREE *ans = create_empty_token(csound); */
    char* op = (char *)csound->Malloc(csound, 7); /* Unchecked */
    TREE *opTree, *bVar;

    switch(gotoNode->type) {
    case KGOTO_TOKEN:
      sprintf(op, "ckgoto");
      break;
    case IGOTO_TOKEN:
      sprintf(op, "cigoto");
      break;
    case ITHEN_TOKEN:
      sprintf(op, "cngoto");
      break;
    case THEN_TOKEN:
    case KTHEN_TOKEN:
      sprintf(op, "cngoto");
      break;
    default:
      if (type) sprintf(op, "ckgoto");
      else sprintf(op, "cggoto");
    }

    opTree = create_opcode_token(csound, op);
    bVar = create_empty_token(csound);
    bVar->type = T_IDENT; //(type ? T_IDENT_B : T_IDENT_b);
    bVar->value = make_token(csound, booleanVar);
    bVar->value->type = bVar->type;

    opTree->left = NULL;
    opTree->right = bVar;
    opTree->right->next = gotoNode->right;

    return opTree;
}

/* THIS PROBABLY NEEDS TO CHANGE TO RETURN DIFFERENT GOTO
   TYPES LIKE IGOTO, ETC */
TREE *create_simple_goto_token(CSOUND *csound, TREE *label, int type)
{
    char* op = (char *)csound->Calloc(csound, 6);
    TREE * opTree;
    char *gt[3] = {"kgoto", "igoto", "goto"};
    sprintf(op, gt[type]);       /* kgoto, igoto, goto ?? */
    opTree = create_opcode_token(csound, op);
    opTree->left = NULL;
    opTree->right = label;

    return opTree;
}

/* Returns true if passed in TREE node is a numerical expression */
int is_expression_node(TREE *node)
{
    if (node == NULL) {
      return 0;
    }

    switch(node->type) {
    case '+':
    case '-':
    case '*':
    case '/':
    case '%':
    case '^':
    case T_FUNCTION:
    case S_UMINUS:
    case '|':
    case '&':
    case S_BITSHIFT_RIGHT:
    case S_BITSHIFT_LEFT:
    case '#':
    case '~':
    case '?':
    case S_TABSLICE:
    case S_TABRANGE:
    case S_TABREF:
    case T_ARRAY:
    case T_MAPK:
    case T_MAPI:
    case T_TADD:
    case S_TUMINUS:
    case T_TMUL:
    case T_TDIV:
    case T_TREM:
    case T_TIMUL:
    case T_TIDIV:
    case T_TIREM:
    case S_A2K:
      return 1;
    }
   return 0;
}

/* Returns if passed in TREE node is a boolean expression */
int is_boolean_expression_node(TREE *node)
{
    if (node == NULL) {
      return 0;
    }

    switch(node->type) {
    case S_EQ:
    case S_NEQ:
    case S_GE:
    case S_LE:
    case S_GT:
    case S_LT:
    case S_AND:
    case S_OR:
      return 1;
    }
    return 0;
}

static TREE *create_cond_expression(CSOUND *csound, TREE *root, int line, int locn)
{
    char arg1, arg2, ans, *outarg = NULL;
    char outype;
    TREE *anchor = create_boolean_expression(csound, root->left, line, locn);
    TREE *last;
    TREE * opTree;
    TREE *b;
    TREE *c = root->right->left, *d = root->right->right;
    last = anchor;
    char condInTypes[4];
    
    while (last->next != NULL) {
      last = last->next;
    }
    b= create_ans_token(csound, last->left->value->lexeme);
    if (is_expression_node(c)) {
      last->next = create_expression(csound, c, line, locn);
      /* TODO - Free memory of old left node
         freetree */
      last = last->next;
      while (last->next != NULL) {
        last = last->next;
      }
      c = create_ans_token(csound, last->left->value->lexeme);
    }
    if (is_expression_node(d)) {
      last->next = create_expression(csound, d, line, locn);
      /* TODO - Free memory of old left node
         freetree */
      last = last->next;
      while (last->next != NULL) {
        last = last->next;
      }
      d = create_ans_token(csound, last->left->value->lexeme);
    }

    arg1 = argtyp2( c->value->lexeme);
    arg2 = argtyp2( d->value->lexeme);
    ans  = argtyp2( b->value->lexeme);
    
    condInTypes[0] = ans;
    condInTypes[1] = arg1;
    condInTypes[2] = arg2;
    condInTypes[3] = 0;
    
    OENTRIES* entries = find_opcode2(csound, ":cond");
    outype = resolve_opcode_get_outarg(csound, entries, condInTypes);
    
    outarg = create_out_arg(csound, outype);
    opTree = create_opcode_token(csound, cs_strdup(csound, ":cond"));
    opTree->left = create_ans_token(csound, outarg);
    opTree->right = b;
    opTree->right->next = c;
    opTree->right->next->next = d;
    /* should recycle memory for root->right */
    //mfree(csound, root->right); root->right = NULL;
    last->next = opTree;
    //    print_tree(csound, "Answer:\n", anchor);
    return anchor;
}
/**
 * Create a chain of Opcode (OPTXT) text from the AST node given. Called from
 * create_opcode when an expression node has been found as an argument
 */
TREE * create_expression(CSOUND *csound, TREE *root, int line, int locn)
{
    char *op, arg1, arg2, c, *outarg = NULL;
    TREE *anchor = NULL, *last;
    TREE * opTree;
    //int opnum;
    OENTRIES* opentries;
    /* HANDLE SUB EXPRESSIONS */

    if (root->type=='?') return create_cond_expression(csound, root, line, locn);

    if (is_expression_node(root->left)) {
      anchor = create_expression(csound, root->left, line, locn);

      /* TODO - Free memory of old left node
         freetree */
      last = anchor;
      while (last->next != NULL) {
        last = last->next;
      }
      root->left = create_ans_token(csound, last->left->value->lexeme);
    }

    if (is_expression_node(root->right)) {
      TREE * newRight = create_expression(csound, root->right, line, locn);
      if (anchor == NULL) {
        anchor = newRight;
      }
      else {
        last = anchor;
        while (last->next != NULL) {
          last = last->next;
        }
        last->next = newRight;
      }
      last = newRight;

      while (last->next != NULL) {
        last = last->next;
      }
        /* TODO - Free memory of old right node
           freetree */
      root->right = create_ans_token(csound, last->left->value->lexeme);
    }
    arg1 = '\0';
    if (root->left != NULL) {
      arg1 = argtyp2( root->left->value->lexeme);
    }
    arg2 = argtyp2( root->right->value->lexeme);
    //printf("arg1=%.2x(%c); arg2=%.2x(%c)\n", arg1, arg1, arg2, arg2);
    op = mcalloc(csound, 80);

    switch(root->type) {
    case '+':
      strncpy(op, "##add", 80);
      outarg = set_expression_type(csound, op, arg1, arg2);
      break;
    case '-':
      strncpy(op, "##sub", 80);
      outarg = set_expression_type(csound, op, arg1, arg2);
      break;
    case '*':
      strncpy(op, "##mul", 80);
      outarg = set_expression_type(csound, op, arg1, arg2);
      break;
    case '%':
      strncpy(op, "##mod", 80);
      outarg = set_expression_type(csound, op, arg1, arg2);
      break;
    case '/':
      strncpy(op, "##div", 80);
      outarg = set_expression_type(csound, op, arg1, arg2);
      break;
    case '^':
      { int outype = 'i';
        strncpy(op, "pow.", 80);
        if (arg1 == 'a' || arg2 == 'a') {
          strncat(op, "a", 80);
          outype = (arg1 == 'a' ? arg1 : arg2);
        }
        else if (arg1 == 'k' || arg2 == 'k') {
          strncat(op, "k", 80);
          outype = (arg1 == 'k' ? arg1 : arg2);
        }
        else
          strncat(op, "i", 80);
        outarg = create_out_arg(csound, outype);
      }
      break;
    case S_TABREF:
      strncpy(op, "##tabref", 80);
      outarg = create_out_arg(csound, 'k');
      break;
    case S_TABRANGE:
      strncpy(op, "#tabgen", 80);
      outarg = create_out_arg(csound, 't');
      break;
    case S_TABSLICE:
      strncpy(op, "#tabslice", 80);
      if (UNLIKELY(PARSER_DEBUG))
        csound->Message(csound, "Found TABSLICE: %s\n", op);
      outarg = create_out_arg(csound, 't');
      break;
    case T_MAPK:
      strncpy(op, "#tabmap", 80);
      if (UNLIKELY(PARSER_DEBUG))
        csound->Message(csound, "Found TABMAP: %s\n", op);
      outarg = create_out_arg(csound, 't');
      break;
    case T_MAPI:
      strncpy(op, "#tabmapo_i", 80);
      if (UNLIKELY(PARSER_DEBUG))
        csound->Message(csound, "Found TABMAP: %s\n", op);
      outarg = create_out_arg(csound, 't');
      break;
    case T_FUNCTION: /* assumes only single arg input */
      c = arg2;
//      if (c == 'p' || c == 'c' || c == 't')   c = 'i';
//      sprintf(op, "%s.%c", root->value->lexeme, c);
      op = cs_strdup(csound, root->value->lexeme);
      if (UNLIKELY(PARSER_DEBUG))
        csound->Message(csound, "Found OP: %s\n", op);
      /* VL: some non-existing functions were appearing here
         looking for opcodes that did not exist */
      opentries = find_opcode2(csound, root->value->lexeme);

      if (opentries->count == 0) {
                                /* This is a little like overkill 
                                 * and also this opnum variable is not used  */
        //opnum = find_opcode_num(csound, "##error", "i", "i");
        csound->Warning(csound,
                    Str("error: function %s with arg type %c not found, "
                        "line %d \n"),
                    root->value->lexeme, c, line);
        c = 'i';
      } else {
          char temp[2];
          temp[0] = c;
          temp[1] = 0;
        c = resolve_opcode_get_outarg(csound, opentries, temp);
      }
      outarg = create_out_arg(csound, c);
      break;
    case S_UMINUS:
      if (UNLIKELY(PARSER_DEBUG))
        csound->Message(csound, "HANDLING UNARY MINUS!");
      root->left = create_minus_token(csound);
      arg1 = 'i';
      strncpy(op, "##mul", 80);
      outarg = set_expression_type(csound, op, arg1, arg2);
      break;
    case '|':
      strncpy(op, "##or", 80);
      outarg = set_expression_type(csound, op, arg1, arg2);
      break;
    case '&':
      strncpy(op, "##and", 80);
      outarg = set_expression_type(csound, op, arg1, arg2);
      break;
    case S_BITSHIFT_RIGHT:
      strncpy(op, "##shr", 80);
      outarg = set_expression_type(csound, op, arg1, arg2);
      break;
    case S_BITSHIFT_LEFT:
      strncpy(op, "##shl", 80);
      outarg = set_expression_type(csound, op, arg1, arg2);
      break;
    case '#':
      strncpy(op, "##xor", 80);
      outarg = set_expression_type(csound, op, arg1, arg2);
      break;
    case '~':
      { int outype = 'i';
        strncpy(op, "##not.", 80);
        if (arg2 == 'a') {
          strncat(op, "a", 80);
          outype = 'a';
        }
        else if (arg2 == 'k') {
          strncat(op, "k", 80);
          outype = 'k';
        }
        else
          strncat(op, "i", 80);
        outarg = create_out_arg(csound, outype);
      }
      break;
     case T_TADD:
      strncpy(op, "##plustab", 80);
      outarg = set_expression_type(csound, op, arg1, arg2);
      break;
     case T_SUB:
      strncpy(op, "##subtab", 80);
      outarg = set_expression_type(csound, op, arg1, arg2);
      break;
     case S_TUMINUS:
      strncpy(op, "##negtab", 80);
      outarg = set_expression_type(csound, op, arg1, arg2);
      break;
     case T_TMUL:
      strncpy(op, "##multtab", 80);
      outarg = set_expression_type(csound, op, arg1, arg2);
      break;
     case T_TDIV:
      strncpy(op, "##divtab", 80);
      outarg = set_expression_type(csound, op, arg1, arg2);
      break;
     case T_TREM:
      strncpy(op, "##remtab", 80);
      outarg = set_expression_type(csound, op, arg1, arg2);
      break;
     case T_TIMUL:
      strncpy(op, "##mulitab", 80);
      outarg = set_expression_type(csound, op, arg1, arg2);
      break;
     case T_TIDIV:
      strncpy(op, "##divitabtab", 80);
      outarg = set_expression_type(csound, op, arg1, arg2);
      break;
     case T_TIREM:
       strncpy(op, "##remitab", 80);
       outarg = set_expression_type(csound, op, arg1, arg2);
       break;
     case S_A2K:
       strncpy(op, "vaget", 80);
       outarg = create_out_arg(csound, 'k');
       break;
     case T_ARRAY:
        strncpy(op, "##array_get", 80);
        outarg = create_out_arg(csound, argtyp2(root->left->value->lexeme));
        break;
    
     }
     opTree = create_opcode_token(csound, op);
     if (root->left != NULL) {
       opTree->right = root->left;
       opTree->right->next = root->right;
       opTree->left = create_ans_token(csound, outarg);
       opTree->line = line;
       opTree->locn = locn;
       //print_tree(csound, "making expression", opTree);
     }
     else {
       opTree->right = root->right;
       opTree->left = create_ans_token(csound, outarg);
       opTree->line = line;
       opTree->locn = locn;
     }
     if (root->type==S_TABRANGE) handle_optional_args(csound, opTree);
     if (anchor == NULL) {
       anchor = opTree;
     }
     else {
       last = anchor;
       while (last->next != NULL) {
         last = last->next;
       }
       last->next = opTree;
     }
    mfree(csound, op);
    return anchor;
}

/**
 * Create a chain of Opcode (OPTXT) text from the AST node given. Called from
 * create_opcode when an expression node has been found as an argument
 */
TREE * create_boolean_expression(CSOUND *csound, TREE *root, int line, int locn)
{
    char *op, *outarg;
    TREE *anchor = NULL, *last;
    TREE * opTree;

    if (UNLIKELY(PARSER_DEBUG)) 
      csound->Message(csound, "Creating boolean expression\n");
    /* HANDLE SUB EXPRESSIONS */
    if (is_boolean_expression_node(root->left)) {
      anchor = create_boolean_expression(csound, root->left, line, locn);
      last = anchor;
      while (last->next != NULL) {
        last = last->next;
      }
      /* TODO - Free memory of old left node
         freetree */
      root->left = create_ans_token(csound, last->left->value->lexeme);
    } else if (is_expression_node(root->left)) {
      anchor = create_expression(csound, root->left, line, locn);

      /* TODO - Free memory of old left node
         freetree */
      last = anchor;
      while (last->next != NULL) {
        last = last->next;
      }
      root->left = create_ans_token(csound, last->left->value->lexeme);
    }


    if (is_boolean_expression_node(root->right)) {
      TREE * newRight = create_boolean_expression(csound, root->right, line, locn);
      if (anchor == NULL) {
        anchor = newRight;
      }
      else {
        last = anchor;
        while (last->next != NULL) {
          last = last->next;
        }
        last->next = newRight;
      }
      last = newRight;

      while (last->next != NULL) {
        last = last->next;
      }
      /* TODO - Free memory of old right node
         freetree */
      root->right = create_ans_token(csound, last->left->value->lexeme);
    }
    else if (is_expression_node(root->right)) {
      TREE * newRight = create_expression(csound, root->right, line, locn);
      if (anchor == NULL) {
        anchor = newRight;
      }
      else {
        last = anchor;
        while (last->next != NULL) {
          last = last->next;
        }
        last->next = newRight;
      }
      last = newRight;

      while (last->next != NULL) {
        last = last->next;
      }

      /* TODO - Free memory of old right node
         freetree */
      root->right = create_ans_token(csound, last->left->value->lexeme);
      root->line = line;
      root->locn = locn;
    }

    op = mcalloc(csound, 80);
    switch(root->type) {
    case S_EQ:
      strncpy(op, "==", 80);
      break;
    case S_NEQ:
      strncpy(op, "!=", 80);
      break;
    case S_GE:
      strncpy(op, ">=", 80);
      break;
    case S_LE:
      strncpy(op, "<=", 80);
      break;
    case S_GT:
      strncpy(op, ">", 80);
      break;
    case S_LT:
      strncpy(op, "<", 80);
      break;
    case S_AND:
      strncpy(op, "&&", 80);
      break;
    case S_OR:
      strncpy(op, "||", 80);
      break;
    }

    if (UNLIKELY(PARSER_DEBUG))
      csound->Message(csound, "Operator Found: %s (%c %c)\n", op,
                      argtyp2( root->left->value->lexeme),
                      argtyp2( root->right->value->lexeme));

    outarg = get_boolean_arg(
                 csound,
                 argtyp2( root->left->value->lexeme) =='k' ||
                 argtyp2( root->right->value->lexeme)=='k' ||
                 argtyp2( root->left->value->lexeme) =='B' ||
                 argtyp2( root->right->value->lexeme)=='B');

    opTree = create_opcode_token(csound, op);
    opTree->right = root->left;
    opTree->right->next = root->right;
    opTree->left = create_ans_token(csound, outarg);
    if (anchor == NULL) {
      anchor = opTree;
    }
    else {
      last = anchor;
      while (last->next != NULL) {
        last = last->next;
      }
      last->next = opTree;
    }
    mfree(csound, op);
    return anchor;
}


static TREE *create_synthetic_ident(CSOUND *csound, int32 count)
{
    char *label = (char *)csound->Calloc(csound, 20);
    ORCTOKEN *token;

    sprintf(label, "__synthetic_%ld", (long)count);
    if (UNLIKELY(PARSER_DEBUG))
      csound->Message(csound, "Creating Synthetic T_IDENT: %s\n", label);
    token = make_token(csound, label);
    token->type = T_IDENT;
    return make_leaf(csound, -1, 0, T_IDENT, token);
}

TREE *create_synthetic_label(CSOUND *csound, int32 count)
{
    char *label = (char *)csound->Calloc(csound, 20);

    sprintf(label, "__synthetic_%ld:", (long)count);
    if (UNLIKELY(PARSER_DEBUG))
      csound->Message(csound, "Creating Synthetic label: %s\n", label);
    return make_leaf(csound, -1, 0, LABEL_TOKEN, make_label(csound, label));
}

/* Expands expression nodes into opcode calls
 *
 *
 * for if-goto, expands to:
 *   1. Expression nodes - all of the expressions that lead to the boolean var
 *   2. goto node - a conditional goto to evals the boolean var and goes to a
 *      label
 *
 * for if-then-elseif-else, expands to:
 *   1. for each conditional, converts to a set of:
 *      -expression nodes
 *      -conditional not-goto that goes to block end label if condition does
 *       not pass (negative version of conditional is used to conditional skip
 *       contents of block)
 *      -statements (body of within conditional block)
 *      -goto complete block end (this signifies that at the end of these
 *       statements, skip all other elseif or else branches and go to very end)
 *      -block end label
 *   2. for else statements found, do no conditional and just link in statements
 *
 * */

TREE *csound_orc_expand_expressions(CSOUND * csound, TREE *root)
{
    //    int32 labelCounter = 300L;

    TREE *anchor = NULL;
    TREE * expressionNodes = NULL;

    TREE *current = root;
    TREE *previous = NULL;

    if (UNLIKELY(PARSER_DEBUG))
      csound->Message(csound, "[Begin Expanding Expressions in AST]\n");

    while (current != NULL) {
      switch(current->type) {
      case INSTR_TOKEN:
        if (UNLIKELY(PARSER_DEBUG))
          csound->Message(csound, "Instrument found\n");
        current->right = csound_orc_expand_expressions(csound, current->right);
        //print_tree(csound, "AFTER", current);
        break;
      case UDO_TOKEN:
        if (UNLIKELY(PARSER_DEBUG)) csound->Message(csound, "UDO found\n");
        current->right = csound_orc_expand_expressions(csound, current->right);
        break;
      case IF_TOKEN:
        if (UNLIKELY(PARSER_DEBUG))
          csound->Message(csound, "Found IF statement\n");
        {
          TREE * left = current->left;
          TREE * right = current->right;
          TREE* last;
          TREE * gotoToken;

          if (right->type == IGOTO_TOKEN ||
              right->type == KGOTO_TOKEN ||
              right->type == GOTO_TOKEN) {
            if (UNLIKELY(PARSER_DEBUG))
              csound->Message(csound, "Found if-goto\n");
            expressionNodes =
              create_boolean_expression(csound, left, right->line, right->locn);
            /* Set as anchor if necessary */
            if (anchor == NULL) {
              anchor = expressionNodes;
            }
            /* reconnect into chain */
            last = expressionNodes;
            while (last->next != NULL) {
              last = last->next;
            }
            if (previous != NULL) {
              previous->next = expressionNodes;
            }
            gotoToken = create_goto_token(csound,
                                          last->left->value->lexeme,
                                          right,
                                          last->left->type == 'k' ||
                                          right->type =='k');
            last->next = gotoToken;
            gotoToken->next = current->next;

            current = gotoToken;
            previous = last;
          }
          else if (right->type == THEN_TOKEN ||
                   right->type == ITHEN_TOKEN ||
                   right->type == KTHEN_TOKEN) {
            int endLabelCounter = -1;
            TREE *tempLeft;
            TREE *tempRight;
            TREE *ifBlockStart = NULL;
            TREE *ifBlockCurrent = current;
            TREE *ifBlockLast = NULL;

            if (UNLIKELY(PARSER_DEBUG))
              csound->Message(csound, "Found if-then\n");
            if (right->next != NULL) {
              endLabelCounter = genlabs++;
            }

            while (ifBlockCurrent != NULL) {
              tempLeft = ifBlockCurrent->left;
              tempRight = ifBlockCurrent->right;
              if (ifBlockCurrent->type == ELSE_TOKEN) {
                //  print_tree(csound, "ELSE case\n", ifBlockCurrent);
                ifBlockLast->next =
                  csound_orc_expand_expressions(csound, tempRight);
                while (ifBlockLast->next != NULL) {
                  ifBlockLast = ifBlockLast->next;
                }
                // print_tree(csound, "ELSE transformed\n", ifBlockCurrent);
                break;
              }
              else if (ifBlockCurrent->type == ELSEIF_TOKEN) { /* JPff code */
                // print_tree(csound, "ELSEIF case\n", ifBlockCurrent);
                ifBlockCurrent->type = IF_TOKEN;
                ifBlockCurrent = make_node(csound, ifBlockCurrent->line,
                                           ifBlockCurrent->locn, ELSE_TOKEN,
                                           NULL, ifBlockCurrent);
                //tempLeft = NULL;
                /*   ifBlockLast->next = */
                /*     csound_orc_expand_expressions(csound, ifBlockCurrent); */
                /* while (ifBlockLast->next != NULL) { */
                /*   ifBlockLast = ifBlockLast->next; */
                /* } */
                // print_tree(csound, "ELSEIF transformed\n", ifBlockCurrent);
                //break;
              }

              expressionNodes =
                create_boolean_expression(csound, tempLeft,
                                          tempLeft->line, tempLeft->locn);
                            /* Set as anchor if necessary */
              if (ifBlockStart == NULL) {
                ifBlockStart = expressionNodes;
              }
              /* reconnect into chain */
              {
                TREE* last = expressionNodes;
                TREE *statements, *label, *labelEnd, *gotoToken;
                int gotoType;

                while (last->next != NULL) {
                  last = last->next;
                }
                if (ifBlockLast != NULL) {
                  ifBlockLast->next = expressionNodes;
                }

                statements = tempRight->right;
                label = create_synthetic_ident(csound, genlabs);
                labelEnd = create_synthetic_label(csound, genlabs++);
                tempRight->right = label;
//              printf("goto types %c %c %c %c %d\n",
//                     expressionNodes->left->type, tempRight->type,
//                     argtyp2( last->left->value->lexeme),
//                     argtyp2( tempRight->value->lexeme),
//                     (argtyp2( last->left->value->lexeme) == 'k') ||
//                     (argtyp2( tempRight->value->lexeme) == 'k'));
//              print_tree(csound, "expression nodes", expressionNodes);
                gotoType = (argtyp2( last->left->value->lexeme) == 'B') ||
                           (argtyp2( tempRight->value->lexeme) == 'k');
                gotoToken =
                  create_goto_token(csound,
                   last->left->value->lexeme,
                   tempRight,
                   gotoType);
                /* relinking */
                last->next = gotoToken;
                gotoToken->next = statements;
                
                if (statements != NULL) {
                  while (statements->next != NULL) {
                    statements = statements->next;
                  }
                }
                if (endLabelCounter > 0) {
                  TREE *endLabel = create_synthetic_ident(csound,
                                                          endLabelCounter);
                  int type = (gotoType == 1) ? 0 : 2;
                  /* csound->DebugMsg(csound, "%s(%d): type = %d %d\n", */
                  /*        __FILE__, __LINE__, type, gotoType); */
                  TREE *gotoEndLabelToken =
                    create_simple_goto_token(csound, endLabel, type);
                  if (UNLIKELY(PARSER_DEBUG))
                    csound->Message(csound, "Creating simple goto token\n");

                  if(statements == NULL) {
                    gotoToken->next = gotoEndLabelToken;
                  } else {
                    statements->next = gotoEndLabelToken;
                  }
                  gotoEndLabelToken->next = labelEnd;
                }
                else {
                  if(statements == NULL) {
                    gotoToken->next = labelEnd;
                  } else {
                    statements->next = labelEnd;
                  }
                }

                ifBlockLast = labelEnd;
                ifBlockCurrent = tempRight->next;
              }
            }

            if (endLabelCounter > 0) {
              TREE *endLabel = create_synthetic_label(csound,
                                                      endLabelCounter);
              endLabel->next = ifBlockLast->next;
              ifBlockLast->next = endLabel;
              ifBlockLast = endLabel;
            }
            ifBlockLast->next = current->next;

            /* Connect in all of the TREE nodes that were flattened from
             * the if-else-else block
             */
            /* Set as anchor if necessary */
            if (anchor == NULL) {
              anchor = ifBlockStart;
            }

            /* reconnect into chain */
            if (previous != NULL) {
              previous->next = ifBlockStart;
            }
            current = ifBlockStart;
          }
          else {
            csound->Message(csound,
                            "ERROR: Neither if-goto or if-then found on line %d!!!",
                            right->line);
          }
        }
        break;
      case UNTIL_TOKEN:
        if (UNLIKELY(PARSER_DEBUG))
          csound->Message(csound, "Found UNTIL statement\n");
        {
          //TREE * left = current->left;
          //TREE * right = current->right;
          //TREE* last;
          TREE * gotoToken;

          int32 topLabelCounter = genlabs++;
          int32 endLabelCounter = genlabs++;
          TREE *tempLeft = current->left;
          TREE *tempRight = current->right;
          //TREE *ifBlockStart = current;
          TREE *ifBlockCurrent = current;
          TREE *ifBlockLast = current;
          //TREE *next = current->next;
          TREE *statements, *labelEnd;
          int type;
          /* *** Stage 1: Create a top label (overwriting *** */
          {                           /* Change UNTIL to label and add IF */
            TREE* t=create_synthetic_label(csound, topLabelCounter);
            current->type = t->type; current->left = current->right = NULL;
            current->value = t->value;
            ifBlockCurrent = t;
            ifBlockCurrent->left = tempLeft;
            ifBlockCurrent->right = tempRight;
            ifBlockCurrent->next = current->next;
            ifBlockCurrent->type = IF_TOKEN;
            current->next = t;
          }
          /* *** Stage 2: Boolean expression *** */
          /* Deal with the boolean expression to variable */
          tempRight = ifBlockCurrent->right;
          expressionNodes =
            ifBlockLast->next = create_boolean_expression(csound,
                                                          ifBlockCurrent->left,
                                                          ifBlockCurrent->line,
                                                          ifBlockCurrent->locn);
          while (ifBlockLast->next != NULL) {
            ifBlockLast = ifBlockLast->next;
          }
          /* *** Stage 3: Create the goto *** */
          statements = tempRight;     /* the body of the loop */
          labelEnd = create_synthetic_label(csound, endLabelCounter);
          gotoToken =
            create_goto_token(csound,
                              ifBlockLast->left->value->lexeme,
                              labelEnd,
                              type =
                              ((argtyp2(
                                  ifBlockLast->left->value->lexeme)=='B')
                               ||
                               (argtyp2(
                                   tempRight->value->lexeme) == 'k')));
          /* relinking */
          /* tempRight = ifBlockLast->next; */
          ifBlockLast->next = gotoToken;
          /* ifBlockLast->next->next = tempRight; */
          gotoToken->right->next = labelEnd;
          gotoToken->next = statements;
          labelEnd = create_synthetic_label(csound, endLabelCounter);
          while (statements->next != NULL) { /* To end of body */
            statements = statements->next;
          }
          {
            TREE *topLabel = create_synthetic_ident(csound,
                                                    topLabelCounter);
            TREE *gotoTopLabelToken =
              create_simple_goto_token(csound, topLabel, (type==1 ? 0 : 1));
            if (UNLIKELY(PARSER_DEBUG))
              csound->Message(csound, "Creating simple goto token\n");
            statements->next = gotoTopLabelToken;
            gotoTopLabelToken->next = labelEnd;
          }
          labelEnd->next = ifBlockCurrent->next;
          ifBlockLast = labelEnd;
          ifBlockCurrent = tempRight->next;
        }
        break;
      case LABEL_TOKEN:
        break;
      case '=':
        {
          TREE* currentArg = current->right;
          TREE* currentAns = current->left;
          char anstype, argtype;

          //csound->Message(csound, "Assignment Statement.\n");
            
          if (currentAns->type == T_ARRAY) {
            anstype = argtyp2(currentAns->left->value->lexeme);
            TREE* temp = create_ans_token(csound,
                                          create_out_arg(csound, anstype));
            current->left = temp;
            
            TREE* arraySet = create_opcode_token(csound, "##array_set");
            arraySet->right = currentAns->left;
            arraySet->right->next = make_leaf(csound, temp->line, temp->locn,
                                                T_IDENT, make_token(csound, temp->value->lexeme));
            arraySet->right->next->next = currentAns->right;
            
            arraySet->next = current->next;
            current->next = arraySet;
              
            currentAns = temp;
            
          }
            
          if (currentArg->left || currentArg->right) {
            //csound->Message(csound, "expansion case\n");
            anstype = argtyp2( currentAns->value->lexeme);
              
            //print_tree(csound, "Assignment\n", current);
            expressionNodes =
              create_expression(csound, currentArg,
                                currentArg->line, currentArg->locn);
            //print_tree(csound, "expressionNodes\n", currentArg);
            currentArg = expressionNodes;
            while (currentArg->next) currentArg = currentArg->next;
            //print_tree(csound, "currentArg\n", currentArg);
            argtype = argtyp2( currentArg->left->value->lexeme);
            //printf("anstype = %c argtype = %c\n", anstype, argtype);
            if (anstype=='a' && argtype!='a') {
              //upsample
              //goto maincase;                     /* Wastes time and space */
              TREE* opTree = create_opcode_token(csound, "upsamp");
              ORCTOKEN* lex = make_token(csound, currentArg->left->value->lexeme);
              //printf("lex %p->%p\n", currentArg->left->value, lex);
              opTree->right = make_leaf(csound, current->line, current->locn,
                                        T_IDENT, lex);
              opTree->left = current->left;
              opTree->line = current->line;
              opTree->locn = current->locn;
              opTree->next = current->next;
              //print_tree(csound, "opTree\n", opTree);
              currentArg->next = opTree;
              //print_tree(csound, "currentArg\n", currentArg);
              //print_tree(csound, "making expression", opTree);

              /* current->right = currentArg->left; /\* Should this copy? *\/ */
              /* current->next = NULL; */
              /* currentArg->next = current; */
              print_tree(csound, "becomes\n", expressionNodes);
              memmove(current, expressionNodes, sizeof(TREE));
              print_tree(csound, "current\n", current);
              break;
            }
            else if (anstype=='k' && argtype=='i') {
              TREE* opTree = create_opcode_token(csound, "=.k");
              ORCTOKEN* lex = make_token(csound, currentArg->left->value->lexeme);
              //printf("value=%p lexeme=%s\n",
              //       currentArg->left->value, currentArg->left->value->lexeme);
              opTree->right = make_leaf(csound, current->line, current->locn,
                                        T_IDENT, lex);
              opTree->left = current->left;
              opTree->line = current->line;
              opTree->locn = current->locn;
              opTree->next = current->next;
              currentArg->next = opTree;
              memmove(current, expressionNodes, sizeof(TREE));
              print_tree(csound, "current\n", current);
              break;
            }
            else {
              mfree(csound, currentArg->left);
              currentArg->left = currentAns;
              currentArg->next = current->next;
              //print_tree(csound, "becomes\n", expressionNodes);
              memmove(current, expressionNodes, sizeof(TREE));
            }
            break;
          }
        }
      default:
        //maincase:
        { /* This is WRONG in optional argsq */
          TREE* previousArg = NULL;
          TREE* currentArg = current->right;
          if (UNLIKELY(PARSER_DEBUG))
            csound->Message(csound, "Found Statement.\n");
          while (currentArg != NULL) {
            TREE* last;
            TREE *nextArg;
            TREE *newArgTree;
            int is_bool = 0;
            if (is_expression_node(currentArg) ||
                (is_bool = is_boolean_expression_node(currentArg))) {
              char * newArg;
              if (UNLIKELY(PARSER_DEBUG))
                csound->Message(csound, "Found Expression.\n");
              if (is_bool == 0) {
                expressionNodes =
                  create_expression(csound, currentArg,
                                    currentArg->line, currentArg->locn);
              }
              else {
                expressionNodes =
                  create_boolean_expression(csound, currentArg,
                                            currentArg->line, currentArg->locn);
              }

              /* Set as anchor if necessary */
              if (anchor == NULL) {
                anchor = expressionNodes;
              }

              /* reconnect into chain */
              last = expressionNodes;
              while (last->next != NULL) {
                last = last->next;
              }
              last->next = current;
              if (previous == NULL) {
                previous = last;
              }
              else {
                previous->next = expressionNodes;
                previous = last;
              }

              newArg = last->left->value->lexeme;

              if (UNLIKELY(PARSER_DEBUG))
                csound->Message(csound, "New Arg: %s\n", newArg);

              /* handle arg replacement of currentArg here */
              nextArg = currentArg->next;
              newArgTree = create_ans_token(csound, newArg);

              if (previousArg == NULL) {
                current->right = newArgTree;
              }
              else {
                previousArg->next = newArgTree;
              }

              newArgTree->next = nextArg;
              currentArg = newArgTree;
              /* TODO - Delete the expression nodes here */
            }

            previousArg = currentArg;
            currentArg = currentArg->next;
          }
          handle_polymorphic_opcode(csound, current);
          handle_optional_args(csound, current);
        }
      }

      if (anchor == NULL) {
        anchor = current;
      }
      previous = current;
      current = current->next;
    }

    if (UNLIKELY(PARSER_DEBUG))
      csound->Message(csound, "[End Expanding Expressions in AST]\n");

    return anchor;
}
