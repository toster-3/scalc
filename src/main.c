#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "da.h"
#include "typedefs.h"

enum { T_EOF, T_NUM, T_OP };
enum { O_PLUS = 1, O_MINUS, O_MUL, O_DIV, O_LPAREN, O_RPAREN };

typedef struct {
	u32 type;
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

void skipspace(char **s)
{
	while (**s == ' ')
		(*s)++;
}

void parse(char *s, Tokens *t)
{
	char *p = s;
	for (; *p;) {
		skipspace(&p);
		if (isdigit(*p) || *p == '.') {
			char *start = p;
			f64 n = strtod(start, &p);
			da_append(t, (Token){0});
			da_last(t) = (Token){.type = T_NUM, .num = n};
			continue;
		}
		da_append(t, (Token){0});
		switch (*p) {
		case '+':
			da_last(t) = (Token){.type = T_OP, .op = O_PLUS};
			break;
		case '-':
			da_last(t) = (Token){.type = T_OP, .op = O_MINUS};
			break;
		case '*':
			da_last(t) = (Token){.type = T_OP, .op = O_MUL};
			break;
		case '/':
			da_last(t) = (Token){.type = T_OP, .op = O_DIV};
			break;
		case '(':
			da_last(t) = (Token){.type = T_OP, .op = O_LPAREN};
			break;
		case ')':
			da_last(t) = (Token){.type = T_OP, .op = O_RPAREN};
			break;
		default:
			printf("unkown character '%c'\n", *p);
			exit(1);
		}
		p++;
	}
	da_append(t, (Token){.type = T_EOF});
}

u8 prefix_binding_power(u8 op)
{
	switch (op) {
	case O_PLUS:
	case O_MINUS:
		return 5;
	default:
		printf("unkown operator '%d'\n", op);
		exit(1);
	}
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

f64 do_op(u8 op, f64 lhs, f64 rhs)
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
	default:
		printf("unkown operator '%d' fart\n", op);
		exit(1);
	}
}

typedef struct {
	Tokens *tokens;
	usize at;
} Lexer;

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
			assert(tmp.type == T_OP && tmp.op == O_RPAREN);
			break;
		}
		lhs.type = T_NUM;
		r_bp = prefix_binding_power(op);
		rhs = expr_bp(l, r_bp);
		if (op == O_PLUS) {
			lhs.num = rhs;
		} else if (op == O_MINUS) {
			lhs.num = -rhs;
		} else {
			printf("yall idk anymore\n");
			exit(1);
		}
		break;
	default:
		printf("bad token a: %d\n", lhs.type);
		exit(1);
	}
	if (lhs.type != T_NUM) {
		printf("bad token a: %d\n", lhs.type);
		exit(1);
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
			printf("bad token: %d\n", t.type);
			exit(1);
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

			lhs.num = do_op(op, lhs.num, rhs);
			continue;
		}

		break;
	}
after_loop:

	return lhs.num;
}

f64 expr(char *s)
{
	Tokens t = {0};
	parse(s, &t);
	/*
	for (usize i = 0; t.buf[i].type != T_EOF; i++) {
	    if (t.buf[i].type == T_OP) {
	        printf("op  %d\n", t.buf[i].op);
	    } else {
	        printf("num %g\n", t.buf[i].num);
	    }
	}
	//*/
	Lexer l = {.tokens = &t, .at = 0};
	f64 ret = expr_bp(&l, 0);
	da_free(t);
	return ret;
}

int main(int argc, char **argv)
{
	char s[1024];

	while (true) {
		fputs("> ", stdout);
		fgets(s, 1024, stdin);
		s[strlen(s) - 1] = '\0';
		printf("%g\n", expr(s));
	}

	return 0;
}
