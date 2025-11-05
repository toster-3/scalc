#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "da.h"
#include "typedefs.h"

#define PROMPT "> "

enum { T_EOF, T_NUM, T_OP };
enum { O_PLUS = 1, O_MINUS, O_MUL, O_DIV, O_POW, O_LPAREN, O_RPAREN };
enum ErrType { ERR_BADCHAR, ERR_BADTOKEN, ERR_BADOP, ERR_NOPAREN };

typedef struct {
	u32 type;
	usize column;
	union {
		f64 num;
		u8 op;
	};
} Token;

typedef struct {
	Token *buf;
	usize len;
	usize cap;
} Tokens;

typedef struct {
	Tokens *tokens;
	char *s;
	usize at;
	usize column;
} Lexer;

jmp_buf env;

usize fillchar(char *s, char c, usize n)
{
	usize i;
	for (i = 0; i < n; i++) {
		s[i] = c;
	}
	s[i] = '\0';
	return n;
}

void error(enum ErrType err, const char *input, usize column)
{
	char s[4096];
	switch (err) {
	case ERR_BADCHAR:
		snprintf(s, 4096, "scalc: unknown character '%c' at position %zu\n%s\n",
		         input[column], column, input);
		break;
	case ERR_BADOP:
		snprintf(s, 4096, "scalc: unknown operator at position %zu\n%s\n",
		         column, input);
		break;
	case ERR_BADTOKEN:
		snprintf(s, 4096, "scalc: unknown token at position %zu\n%s\n", column,
		         input);
		break;
	case ERR_NOPAREN:
		snprintf(s, 4096,
		         "scalc: missing closing parenthesis at position %zu\n%s\n",
		         column, input);
		break;
	}
	fillchar(s + strlen(s), ' ', column);
	strcat(s, "^\n");
	fputs(s, stderr);
	longjmp(env, 1);
}

void parse(char *s, Tokens *t)
{
	char *p = s;
	if (!strcmp(s, "exit") || !strcmp(s, "quit") || !strcmp(s, "q")) {
		da_free(*t);
		exit(0);
	}
	for (; *p;) {
		while (*p == ' ')
			p++;
		if (isdigit(*p) || *p == '.') {
			char *start = p;
			f64 n = strtod(start, &p);
			da_append(t, (Token){0});
			da_last(t) = (Token){.type = T_NUM, .num = n, .column = p - s};
			continue;
		} else if (!*p) {
			return;
		}
		da_append(t, (Token){0});
		switch (*p) {
		case '+':
			da_last(t).op = O_PLUS;
			break;
		case '-':
			da_last(t).op = O_MINUS;
			break;
		case '*':
			da_last(t).op = O_MUL;
			break;
		case '/':
			da_last(t).op = O_DIV;
			break;
		case '^':
			da_last(t).op = O_POW;
		case '(':
			da_last(t).op = O_LPAREN;
			break;
		case ')':
			da_last(t).op = O_RPAREN;
			break;
		default:
			error(ERR_BADCHAR, s, p - s);
		}
		da_last(t).column = p - s;
		da_last(t).type = T_OP;
		p++;
	}
	da_append(t, (Token){.type = T_EOF});
}

u8 prefix_binding_power(u8 op, const char *s, usize at)
{
	switch (op) {
	case O_PLUS:
	case O_MINUS:
		return 5;
	default:
		error(ERR_BADOP, s, at);
	}
	return 0;
}

bool infix_binding_power(u8 op, u8 *l_bp, u8 *r_bp)
{
	switch (op) {
	case O_PLUS:
	case O_MINUS:
		*l_bp = 1;
		*r_bp = 2;
		return true;
	case O_MUL:
	case O_DIV:
		*l_bp = 3;
		*r_bp = 4;
		return true;
	default:
		return false;
	}
}

f64 do_op(u8 op, f64 lhs, f64 rhs, const char *s, usize at)
{
	switch (op) {
	case O_PLUS:
		return lhs + rhs;
	case O_MINUS:
		return lhs - rhs;
	case O_MUL:
		return lhs * rhs;
	case O_DIV:
		return lhs / rhs;
	case O_POW:
		return pow(lhs, rhs);
	default:
		error(ERR_BADOP, s, at);
		return 0;
	}
}

Token lexpeek(Lexer *l)
{
	if (l->at >= l->tokens->len)
		return (Token){.type = T_EOF};
	return l->tokens->buf[l->at];
}

Token lexnext(Lexer *l)
{
	if (l->at >= l->tokens->len) {
		l->at++;
		return (Token){.type = T_EOF};
	}
	l->column = l->tokens->buf[l->at].column;
	return l->tokens->buf[l->at++];
}

f64 expr_bp(Lexer *l, u8 min_bp)
{
	Token lhs = lexnext(l), tmp;
	u8 op, l_bp, r_bp;
	f64 rhs;
	switch (lhs.type) {
	case T_NUM:
		break;
	case T_OP:
		op = lhs.op;
		if (op == O_LPAREN) {
			lhs.type = T_NUM;
			lhs.num = expr_bp(l, 0);
			tmp = lexnext(l);
			if (tmp.type != T_OP || tmp.op != O_RPAREN) {
				error(ERR_NOPAREN, l->s, l->column);
			}
			break;
		}
		lhs.type = T_NUM;
		r_bp = prefix_binding_power(op, l->s, l->column);
		rhs = expr_bp(l, r_bp);
		if (op == O_PLUS) {
			lhs.num = rhs;
		} else if (op == O_MINUS) {
			lhs.num = -rhs;
		}
		break;
	default:
		error(ERR_BADTOKEN, l->s, l->column);
	}
	if (lhs.type != T_NUM) {
		error(ERR_BADTOKEN, l->s, l->column);
	}

	while (1) {
		bool implicit_mul = false;
		Token t = lexpeek(l);
		switch (t.type) {
		case T_EOF:
			goto after_loop;
		case T_OP:
			op = t.op;
			break;
		case T_NUM:
			op = O_MUL;
			implicit_mul = true;
			break;
		default:
			error(ERR_BADTOKEN, l->s, l->column);
		}
		if (infix_binding_power(op, &l_bp, &r_bp) || op == O_LPAREN) {
			if (op == O_LPAREN) {
				op = O_MUL;
				infix_binding_power(op, &l_bp, &r_bp);
				implicit_mul = true;
			}

			if (l_bp < min_bp) {
				break;
			}
			if (!implicit_mul) {
				lexnext(l);
			}
			rhs = expr_bp(l, r_bp);

			lhs.num = do_op(op, lhs.num, rhs, l->s, l->column);
			continue;
		}

		break;
	}
after_loop:

	return lhs.num;
}

void expr(char *s, Tokens *t)
{
	parse(s, t);
	/*
	for (usize i = 0; t.buf[i].type != T_EOF; i++) {
	    if (t.buf[i].type == T_OP) {
	        printf("op  %d\n", t.buf[i].op);
	    } else {
	        printf("num %g\n", t.buf[i].num);
	    }
	}
	//*/
	Lexer l = {.tokens = t, .at = 0, .s = s, .column = 0};
	if (setjmp(env) == 0) {
		printf("%g\n", expr_bp(&l, 0));
	}
	t->len = 0;
}

int main(int argc, char **argv)
{
	char s[1024];
	Tokens t = {0};
	int n;

	while (true) {
		fputs(PROMPT, stdout);
		if (!fgets(s, 1024, stdin)) {
			putchar('\n');
			return 0;
		}
		n = strcspn(s, "\n");
		if (n == 0)
			continue;
		s[n] = '\0';
		expr(s, &t);
	}

	return 0;
}
