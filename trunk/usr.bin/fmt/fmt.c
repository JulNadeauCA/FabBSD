/*	$OpenBSD: fmt.c,v 1.26 2007/03/20 03:50:39 tedu Exp $	*/

/* Sensible version of fmt
 *
 * Syntax: fmt [ options ] [ goal [ max ] ] [ filename ... ]
 *
 * Since the documentation for the original fmt is so poor, here
 * is an accurate description of what this one does. It's usually
 * the same. The *mechanism* used may differ from that suggested
 * here. Note that we are *not* entirely compatible with fmt,
 * because fmt gets so many things wrong.
 *
 * 1. Tabs are expanded, assuming 8-space tab stops.
 *    If the `-t <n>' option is given, we assume <n>-space
 *    tab stops instead.
 *    Trailing blanks are removed from all lines.
 *    x\b == nothing, for any x other than \b.
 *    Other control characters are simply stripped. This
 *    includes \r.
 * 2. Each line is split into leading whitespace and
 *    everything else. Maximal consecutive sequences of
 *    lines with the same leading whitespace are considered
 *    to form paragraphs, except that a blank line is always
 *    a paragraph to itself.
 *    If the `-p' option is given then the first line of a
 *    paragraph is permitted to have indentation different
 *    from that of the other lines.
 *    If the `-m' option is given then a line that looks
 *    like a mail message header, if it is not immediately
 *    preceded by a non-blank non-message-header line, is
 *    taken to start a new paragraph, which also contains
 *    any subsequent lines with non-empty leading whitespace.
 *    Unless the `-n' option is given, lines beginning with
 *    a . (dot) are not formatted.
 * 3. The "everything else" is split into words; a word
 *    includes its trailing whitespace, and a word at the
 *    end of a line is deemed to be followed by a single
 *    space, or two spaces if it ends with a sentence-end
 *    character. (See the `-d' option for how to change that.)
 *    If the `-s' option has been given, then a word's trailing
 *    whitespace is replaced by what it would have had if it
 *    had occurred at end of line.
 * 4. Each paragraph is sent to standard output as follows.
 *    We output the leading whitespace, and then enough words
 *    to make the line length as near as possible to the goal
 *    without exceeding the maximum. (If a single word would
 *    exceed the maximum, we output that anyway.) Of course
 *    the trailing whitespace of the last word is ignored.
 *    We then emit a newline and start again if there are any
 *    words left.
 *    Note that for a blank line this translates as "We emit
 *    a newline".
 *    If the `-l <n>' option is given, then leading whitespace
 *    is modified slightly: <n> spaces are replaced by a tab.
 *    Indented paragraphs (see above under `-p') make matters
 *    more complicated than this suggests. Actually every paragraph
 *    has two `leading whitespace' values; the value for the first
 *    line, and the value for the most recent line. (While processing
 *    the first line, the two are equal. When `-p' has not been
 *    given, they are always equal.) The leading whitespace
 *    actually output is that of the first line (for the first
 *    line of *output*) or that of the most recent line (for
 *    all other lines of output).
 *    When `-m' has been given, message header paragraphs are
 *    taken as having first-leading-whitespace empty and
 *    subsequent-leading-whitespace two spaces.
 *
 * Multiple input files are formatted one at a time, so that a file
 * never ends in the middle of a line.
 *
 * There's an alternative mode of operation, invoked by giving
 * the `-c' option. In that case we just center every line,
 * and most of the other options are ignored. This should
 * really be in a separate program, but we must stay compatible
 * with old `fmt'.
 *
 * QUERY: Should `-m' also try to do the right thing with quoted text?
 * QUERY: `-b' to treat backslashed whitespace as old `fmt' does?
 * QUERY: Option meaning `never join lines'?
 * QUERY: Option meaning `split in mid-word to avoid overlong lines'?
 * (Those last two might not be useful, since we have `fold'.)
 *
 * Differences from old `fmt':
 *
 *   - We have many more options. Options that aren't understood
 *     generate a lengthy usage message, rather than being
 *     treated as filenames.
 *   - Even with `-m', our handling of message headers is
 *     significantly different. (And much better.)
 *   - We don't treat `\ ' as non-word-breaking.
 *   - Downward changes of indentation start new paragraphs
 *     for us, as well as upward. (I think old `fmt' behaves
 *     in the way it does in order to allow indented paragraphs,
 *     but this is a broken way of making indented paragraphs
 *     behave right.)
 *   - Given the choice of going over or under |goal_length|
 *     by the same amount, we go over; old `fmt' goes under.
 *   - We treat `?' as ending a sentence, and not `:'. Old `fmt'
 *     does the reverse.
 *   - We return approved return codes. Old `fmt' returns
 *     1 for some errors, and *the number of unopenable files*
 *     when that was all that went wrong.
 *   - We have fewer crashes and more helpful error messages.
 *   - We don't turn spaces into tabs at starts of lines unless
 *     specifically requested.
 *   - New `fmt' is somewhat smaller and slightly faster than
 *     old `fmt'.
 *
 * Bugs:
 *
 *   None known. There probably are some, though.
 *
 * Portability:
 *
 *   I believe this code to be pretty portable. It does require
 *   that you have `getopt'. If you need to include "getopt.h"
 *   for this (e.g., if your system didn't come with `getopt'
 *   and you installed it yourself) then you should arrange for
 *   NEED_getopt_h to be #defined.
 *
 *   Everything here should work OK even on nasty 16-bit
 *   machines and nice 64-bit ones. However, it's only really
 *   been tested on my FreeBSD machine. Your mileage may vary.
 */

/* Copyright (c) 1997 Gareth McCaughan. All rights reserved.
 *
 * Redistribution and use of this code, in source or binary forms,
 * with or without modification, are permitted subject to the following
 * conditions:
 *
 *  - Redistribution of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *  - If you distribute modified source code it must also include
 *    a notice saying that it has been modified, and giving a brief
 *    description of what changes have been made.
 *
 * Disclaimer: I am not responsible for the results of using this code.
 *             If it formats your hard disc, sends obscene messages to
 *             your boss and kills your children then that's your problem
 *             not mine. I give absolutely no warranty of any sort as to
 *             what the program will do, and absolutely refuse to be held
 *             liable for any consequences of your using it.
 *             Thank you. Have a nice day.
 */

/* RCS change log:
 * Revision 1.5  1998/03/02 18:02:21  gjm11
 * Minor changes for portability.
 *
 * Revision 1.4  1997/10/01 11:51:28  gjm11
 * Repair broken indented-paragraph handling.
 * Add mail message header stuff.
 * Improve comments and layout.
 * Make usable with non-BSD systems.
 * Add revision display to usage message.
 *
 * Revision 1.3  1997/09/30 16:24:47  gjm11
 * Add copyright notice, rcsid string and log message.
 *
 * Revision 1.2  1997/09/30 16:13:39  gjm11
 * Add options: -d <chars>, -l <width>, -p, -s, -t <width>, -h .
 * Parse options with `getopt'. Clean up code generally.
 * Make comments more accurate.
 *
 * Revision 1.1  1997/09/30 11:29:57  gjm11
 * Initial revision
 */

#ifndef lint
static const char rcsid[] =
  "$OpenBSD: fmt.c,v 1.26 2007/03/20 03:50:39 tedu Exp $";
static const char copyright[] =
  "Copyright (c) 1997 Gareth McCaughan. All rights reserved.\n";
#endif /* not lint */

#include <ctype.h>
#include <err.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

/* Something that, we hope, will never be a genuine line length,
 * indentation etc.
 */
#define SILLY ((size_t)-1)

/* I used to use |strtoul| for this, but (1) not all systems have it
 * and (2) it's probably better to use |strtol| to detect negative
 * numbers better.
 * If |fussyp==0| then we don't complain about non-numbers
 * (returning 0 instead), but we do complain about bad numbers.
 */
static size_t
get_positive(const char *s, const char *err_mess, int fussyP)
{
	char *t;
	long result = strtol(s, &t, 0);

	if (*t) {
		if (fussyP)
			goto Lose;
		else
			return 0;
	}
	if (result <= 0) {
Lose:
		errx(EX_USAGE, "%s", err_mess);
	}

	return (size_t) result;
}

/* Global variables */

static int centerP = 0;				/* Try to center lines? */
static size_t goal_length = 0;			/* Target length for output lines */
static size_t max_length = 0;			/* Maximum length for output lines */
static int coalesce_spaces_P = 0;		/* Coalesce multiple whitespace -> ' ' ? */
static int allow_indented_paragraphs = 0;	/* Can first line have diff. ind.? */
static int tab_width = 8;			/* Number of spaces per tab stop */
static size_t output_tab_width = 0;		/* Ditto, when squashing leading spaces */
static const char *sentence_enders = ".?!";	/* Double-space after these */
static int grok_mail_headers = 0;		/* treat embedded mail headers magically? */
static int format_troff = 0;			/* Format troff? */

static int n_errors = 0;			/* Number of failed files. Return on exit. */
static char *output_buffer = NULL;		/* Output line will be built here */
static size_t x;				/* Horizontal position in output line */
static size_t x0;				/* Ditto, ignoring leading whitespace */
static size_t pending_spaces;			/* Spaces to add before next word */
static int output_in_paragraph = 0;		/* Any of current para written out yet? */

/* Prototypes */

static void	process_named_file(const char *);
static void	process_stream(FILE *, const char *);
static size_t	indent_length(const char *, size_t);
static int	might_be_header(const unsigned char *);
static void	new_paragraph(size_t, size_t);
static void	output_word(size_t, size_t, const char *, size_t, size_t);
static void	output_indent(size_t);
static void	center_stream(FILE *, const char *);
static char	*get_line(FILE *, size_t *);
static void	*xrealloc(void *, size_t);
void		usage(void);

#define XMALLOC(x) xrealloc(0, x)

/* Here is perhaps the right place to mention that this code is
 * all in top-down order. Hence, |main| comes first.
 */
int
main(int argc, char *argv[])
{
	int ch;			/* used for |getopt| processing */

	(void)setlocale(LC_CTYPE, "");

	/* 1. Grok parameters. */
	while ((ch = getopt(argc, argv, "0123456789cd:hl:mnpst:w:")) != -1) {
		switch (ch) {
		case 'c':
			centerP = 1;
			break;
		case 'd':
			sentence_enders = optarg;
			break;
		case 'l':
			output_tab_width
				= get_positive(optarg, "output tab width must be positive", 1);
			break;
		case 'm':
			grok_mail_headers = 1;
			break;
		case 'n':
			format_troff = 1;
			break;
		case 'p':
			allow_indented_paragraphs = 1;
			break;
		case 's':
			coalesce_spaces_P = 1;
			break;
		case 't':
			tab_width = get_positive(optarg, "tab width must be positive", 1);
			break;
		case 'w':
			goal_length = get_positive(optarg, "width must be positive", 1);
			max_length = goal_length;
			break;
		case '0': case '1': case '2': case '3': case '4': case '5':
		case '6': case '7': case '8': case '9':
			/* XXX  this is not a stylistically approved use of getopt() */
			if (goal_length == 0) {
				char *p;

				p = argv[optind - 1];
				if (p[0] == '-' && p[1] == ch && !p[2])
					goal_length = get_positive(++p, "width must be nonzero", 1);
				else
					goal_length = get_positive(argv[optind]+1,
							"width must be nonzero", 1);
				max_length = goal_length;
			}
			break;
		case 'h':
		default:
			usage();
			/* NOT REACHED */
		}
	}

	argc -= optind;
	argv += optind;

	/* [ goal [ maximum ] ] */
	if (argc > 0 && goal_length == 0 &&
	    (goal_length = get_positive(*argv,"goal length must be positive", 0)) != 0) {
		--argc;
		++argv;
		if (argc > 0 && (max_length = get_positive(*argv,"max length must be positive", 0)) != 0) {
			--argc;
			++argv;
			if (max_length < goal_length)
				errx(EX_USAGE, "max length must be >= goal length");
		}
	}

	if (goal_length == 0)
		goal_length = 65;
	if (max_length == 0)
		max_length = goal_length+10;
	output_buffer = XMALLOC(max_length+1);	/* really needn't be longer */

	/* 2. Process files. */

	if (argc > 0) {
		while (argc-- > 0)
			process_named_file(*argv++);
	} else {
		process_stream(stdin, "standard input");
	}

	/* We're done. */
	return n_errors ? EX_NOINPUT : 0;

}

/* Process a single file, given its name.
 */
static void
process_named_file(const char *name)
{
	FILE *f;

	if ((f = fopen(name, "r")) == NULL) {
		warn("%s", name);
		++n_errors;
	} else {
		process_stream(f, name);
		fclose(f);
	}
}

/* Types of mail header continuation lines:
 */
typedef enum {
	hdr_ParagraphStart	= -1,
	hdr_NonHeader		= 0,
	hdr_Header		= 1,
	hdr_Continuation	= 2
} HdrType;

/* Process a stream. This is where the real work happens,
 * except that centering is handled separately.
 */
static void
process_stream(FILE *stream, const char *name)
{
	size_t n;
	size_t np;
	size_t last_indent = SILLY;	/* how many spaces in last indent? */
	size_t para_line_number = 0;	/* how many lines already read in this para? */
	size_t first_indent = SILLY;	/* indentation of line 0 of paragraph */
	HdrType prev_header_type = hdr_ParagraphStart;
	HdrType header_type;

	/* ^-- header_type of previous line; -1 at para start */
	char *line;
	size_t length;

	if (centerP) {
		center_stream(stream, name);
		return;
	}

	while ((line = get_line(stream, &length)) != NULL) {
		np = indent_length(line, length);
		header_type = hdr_NonHeader;
		if (grok_mail_headers && prev_header_type != hdr_NonHeader) {
			if (np == 0 && might_be_header(line))
				header_type = hdr_Header;
			else if (np > 0 && prev_header_type>hdr_NonHeader)
				header_type = hdr_Continuation;
		}

		/* We need a new paragraph if and only if:
		 *   this line is blank,
		 *   OR it's a troff request,
		 *   OR it's a mail header,
		 *   OR it's not a mail header AND the last line was one,
		 *   OR the indentation has changed
		 *      AND the line isn't a mail header continuation line
		 *      AND this isn't the second line of an indented paragraph.
		 */
		if (length == 0 || (line[0] == '.' && !format_troff) ||
		    header_type == hdr_Header ||
		    (header_type == hdr_NonHeader && prev_header_type > hdr_NonHeader) ||
		    (np != last_indent && header_type != hdr_Continuation &&
		    (!allow_indented_paragraphs || para_line_number != 1)) ) {
			new_paragraph(output_in_paragraph ? last_indent : first_indent, np);
			para_line_number = 0;
			first_indent = np;
			last_indent = np;

			/* nroff compatibility */
			if (length > 0 && line[0] == '.' && !format_troff) {
				printf("%.*s\n", (int)length, line);
				continue;
			}
			if (header_type == hdr_Header)
				last_indent = 2;	/* for cont. lines */
			if (length == 0) {
				putchar('\n');
				prev_header_type = hdr_ParagraphStart;
				continue;
			} else {
				/* If this is an indented paragraph other than a mail header
				 * continuation, set |last_indent|.
				 */
				if (np != last_indent && header_type != hdr_Continuation)
					last_indent = np;
			}
			prev_header_type = header_type;
		}

		n = np;
		while (n < length) {
			/* Find word end and count spaces after it */
			size_t word_length = 0, space_length = 0;
			while (n+word_length < length && line[n+word_length] != ' ')
				++word_length;
			space_length = word_length;
			while (n+space_length < length && line[n+space_length] == ' ')
				++space_length;
			/* Send the word to the output machinery. */
			output_word(first_indent, last_indent,
				line+n, word_length, space_length-word_length);
			n += space_length;
		}
		++para_line_number;
	}

	new_paragraph(output_in_paragraph ? last_indent : first_indent, 0);
	if (ferror(stream)) {
		warn("%s", name);
		++n_errors;
	}
}

/* How long is the indent on this line?
 */
static size_t
indent_length(const char *line, size_t length)
{
	size_t n = 0;

	while (n < length && *line++ == ' ')
		++n;
	return n;
}

/* Might this line be a mail header?
 * We deem a line to be a possible header if it matches the
 * Perl regexp /^[A-Z][-A-Za-z0-9]*:\s/. This is *not* the same
 * as in RFC whatever-number-it-is; we want to be gratuitously
 * conservative to avoid mangling ordinary civilised text.
 */
static int
might_be_header(const unsigned char *line)
{

	if (!isupper(*line++))
		return 0;
	while (isalnum(*line) || *line == '-')
		++line;
	return (*line == ':' && isspace(line[1]));
}

/* Begin a new paragraph with an indent of |indent| spaces.
 */
static void
new_paragraph(size_t old_indent, size_t indent)
{

	if (x0) {
		if (old_indent > 0)
			output_indent(old_indent);
		fwrite(output_buffer, 1, x0, stdout);
		putchar('\n');
	}
	x = indent;
	x0 = 0;
	pending_spaces = 0;
	output_in_paragraph = 0;
}

/* Output spaces or tabs for leading indentation.
 */
static void
output_indent(size_t n_spaces)
{

	if (output_tab_width) {
		while (n_spaces >= output_tab_width) {
			putchar('\t');
			n_spaces -= output_tab_width;
		}
	}
	while (n_spaces-- > 0)
		putchar(' ');
}

/* Output a single word, or add it to the buffer.
 * indent0 and indent1 are the indents to use on the first and subsequent
 * lines of a paragraph. They'll often be the same, of course.
 */
static void
output_word(size_t indent0, size_t indent1, const char *word, size_t length, size_t spaces)
{
	size_t new_x = x + pending_spaces + length;
	size_t indent = output_in_paragraph ? indent1 : indent0;

	/* If either |spaces==0| (at end of line) or |coalesce_spaces_P|
	 * (squashing internal whitespace), then add just one space;
	 * except that if the last character was a sentence-ender we
	 * actually add two spaces.
	 */
	if (coalesce_spaces_P || spaces == 0)
		spaces = strchr(sentence_enders, word[length-1]) ? 2 : 1;

	if (new_x <= goal_length) {
		/* After adding the word we still aren't at the goal length,
		 * so clearly we add it to the buffer rather than outputing it.
		 */
		memset(output_buffer+x0, ' ', pending_spaces);
		x0 += pending_spaces;
		x += pending_spaces;
		memcpy(output_buffer+x0, word, length);
		x0 += length;
		x += length;
		pending_spaces = spaces;
	} else {
		/* Adding the word takes us past the goal. Print the line-so-far,
		 * and the word too iff either (1) the lsf is empty or (2) that
		 * makes us nearer the goal but doesn't take us over the limit,
		 * or (3) the word on its own takes us over the limit.
		 * In case (3) we put a newline in between.
		 */
		if (indent > 0)
			output_indent(indent);
		fwrite(output_buffer, 1, x0, stdout);
		if (x0 == 0 || (new_x <= max_length && new_x-goal_length <= goal_length-x)) {
			printf("%*s", (int)pending_spaces, "");
			goto write_out_word;
		} else {
			/* If the word takes us over the limit on its own, just
			 * spit it out and don't bother buffering it.
			 */
			if (indent+length > max_length) {
				putchar('\n');
				if (indent > 0)
					output_indent(indent);
write_out_word:
				fwrite(word, 1, length, stdout);
				x0 = 0;
				x = indent1;
				pending_spaces = 0;
			} else {
				memcpy(output_buffer, word, length);
				x0 = length;
				x = length+indent1;
				pending_spaces = spaces;
			}
		}

		putchar('\n');
		output_in_paragraph = 1;
	}
}

/* Process a stream, but just center its lines rather than trying to
 * format them neatly.
 */
static void
center_stream(FILE *stream, const char *name)
{
	char *line;
	size_t length;
	size_t l;

	while ((line = get_line(stream, &length)) != 0) {
		l = length;
		while (l > 0 && isspace(*line)) {
			++line;
			--l;
		}

		length = l;

		while (l < goal_length) {
			putchar(' ');
			l += 2;
		}

		fwrite(line, 1, length, stdout);
		putchar('\n');
	}

	if (ferror(stream)) {
		warn("%s", name);
		++n_errors;
	}
}

/* Get a single line from a stream. Expand tabs, strip control
 * characters and trailing whitespace, and handle backspaces.
 * Return the address of the buffer containing the line, and
 * put the length of the line in |lengthp|.
 * This can cope with arbitrarily long lines, and with lines
 * without terminating \n.
 * If there are no characters left or an error happens, we
 * return 0.
 * Don't confuse |spaces_pending| here with the global
 * |pending_spaces|.
 */
static char *
get_line(FILE *stream, size_t *lengthp)
{
	int ch;
	int troff = 0;
	static char *buf = NULL;
	static size_t length = 0;
	size_t len = 0;
	size_t spaces_pending = 0;

	if (buf == NULL) {
		length = 100;
		buf = XMALLOC(length);
	}

	while ((ch = getc(stream)) != '\n' && ch != EOF) {
		if ((len + spaces_pending == 0) && (ch == '.' && !format_troff))
			troff = 1;
		if (ch == ' ') {
			++spaces_pending;
		} else if (troff || !iscntrl(ch)) {
			while (len + spaces_pending >= length) {
				length *= 2;
				buf = xrealloc(buf, length);
			}

			while (spaces_pending > 0) {
				--spaces_pending;
				buf[len++] = ' ';
			}
			buf[len++] = ch;
		} else if (ch == '\t') {
			spaces_pending += tab_width - (len+spaces_pending)%tab_width;
		} else if (ch == '\b') {
			if (len)
				--len;
		}
	}

	*lengthp = len;
	return (len > 0 || ch != EOF) ? buf : 0;
}

/* (Re)allocate some memory, exiting with an error if we can't.
 */
static void *
xrealloc(void *ptr, size_t nbytes)
{
	void *p;

	p  = realloc(ptr, nbytes);
	if (p == NULL)
		errx(EX_OSERR, "out of memory");
	return p;
}

void
usage(void)
{
	extern char *__progname;

	fprintf(stderr,
		"usage: %s [-cmnps] [-d chars] [-l number] [-t number]\n"
		"\t[goal [maximum] | -width | -w width] [file ...]\n",
			__progname);
	exit (1);
}
