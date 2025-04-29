#include "../config.h"
#include "mosaic.h"
#include "gui.h"
#include "comment.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/utsname.h>

#ifndef DISABLE_TRACE
extern int srcTrace;
#endif

extern mo_window *current_win;
extern char *machine;
extern struct utsname mo_uname;

#define COMMENT_CARD_FILENAME ".mosaic-cc-"
#define COMMENT_TIME 5
#define MO_COMMENT_OS "Not Supported"

#ifdef DEBUG_CC
int do_comment = 1;
#else
int do_comment = 0;
#endif

const char *comment_card_html_top =
"<title>\n"
"  Comment Card for Mosaic 2.6\n"
"</title>\n"
"<h1 align=center>\n"
"  Please Help Us Help You!!\n"
"</h1>\n"
"<hr>\n"
"<h2>\n"
"  Thank you for using NCSA Mosaic! We are continually striving to\n"
"  improve Mosaic to better meet the needs of its users. We would\n"
"  appreciate your taking the time to answer these few questions.\n"
"</h2>\n"
"<hr>\n"
"<form method=\"POST\" action=\"http://sdg.ncsa.uiuc.edu/XCGI/comment26\">\n"
"<h3>\n"
"  <ul>\n"
"    <li>\n"
"      If you do not like surveys or you have already completed this survey,\n"
"      please press this button,\n"
"      <input type=\"submit\" value=\"Just Count Me\" name=\"countme\">,\n"
"      to be counted. Pushing the above button will send the following information\n"
"      about your system to be used in our statistics (completely anonymous):\n"
"      <p>\n";

const char *comment_card_html_bot =
"      </p>\n"
"    </li>\n"
"    <br>\n"
"    <li>If you do not want to fill out this card, just push the \"Close Window\" button at the bottom of this window.</li>\n"
"    <br>\n"
"    <li>Otherwise, please proceed!</li>\n"
"  </ul>\n"
"</h3>\n"
"<hr>\n"
"<p>How long have you been using Mosaic?<br>\n"
"  <select name=\"usage\">\n"
"    <option value=\"no comment\" selected>No Comment</option>\n"
"    <option value=\"never\">Never</option>\n"
"    <option value=\"lt 1 mon\">Less Than 1 Month</option>\n"
"    <option value=\"1-6 mon\">1 - 6 Months</option>\n"
"    <option value=\"6 mon-1 yr\">6 Months to a Year</option>\n"
"    <option value=\"1-2 yrs\">1 - 2 Years</option>\n"
"    <option value=\"gt 2 yrs\">More Than 2 Years</option>\n"
"  </select>\n"
"</p>\n"
"<p>How familiar are you with the World Wide Web?<br>\n"
"  <select name=\"www\">\n"
"    <option value=\"no comment\" selected>No Comment</option>\n"
"    <option value=\"no experience\">No Experience</option>\n"
"    <option value=\"novice\">Novice</option>\n"
"    <option value=\"intermediate\">Intermediate</option>\n"
"    <option value=\"expert\">Expert</option>\n"
"    <option value=\"master\">Web Master</option>\n"
"  </select>\n"
"</p>\n"
"<p>What is your <b>favorite</b> Web browser?<br>\n"
"  <select name=\"favorite\" size=5>\n"
"    <option value=\"no comment\" selected>No Comment</option>\n"
"    <option value=\"arena\">Arena</option>\n"
"    <option value=\"emacs-w3\">Emacs-W3</option>\n"
"    <option value=\"spyglass\">Enhanced Mosaic (Spyglass)</option>\n"
"    <option value=\"hot java\">Hot Java</option>\n"
"    <option value=\"lynx\">Lynx</option>\n"
"    <option value=\"ncsa\">N.C.S.A. Mosaic</option>\n"
"    <option value=\"netscape $$$\">Netscape (for $$$)</option>\n"
"    <option value=\"netscape free\">Netscape (for free)</option>\n"
"    <option value=\"viola\">Viola</option>\n"
"  </select>\n"
"</p>\n"
"<p>Other comments:<br>\n"
"<textarea name=\"comments_feedback\" rows=5 cols=60></textarea>\n"
"</p>\n"
"<p><input type=\"submit\" value=\"Submit Comment Card for X Mosaic\" name=\"submitme\"></p>\n"
"</form>\n";


char *MakeFilename(void) {
    char *hptr, home[256], *fname;
    struct passwd *pwdent;

    if (!(hptr = getenv("HOME"))) {
        if (!(pwdent = getpwuid(getuid()))) return NULL;
        strcpy(home, pwdent->pw_dir);
    } else {
        strcpy(home, hptr);
    }

    fname = (char *)calloc(strlen(home) + strlen(COMMENT_CARD_FILENAME) + 10, sizeof(char));
    sprintf(fname, "%s/%s%s", home, COMMENT_CARD_FILENAME, "2.6");
    return fname;
}

void InitCard(char *fname) {
    FILE *fp = fopen(fname, "w");
    long num[2] = {1, 0};
    if (fp) {
        fwrite(num, sizeof(long), 2, fp);
        fclose(fp);
    }
}

void PutCardCount(long *num, char *fname) {
    FILE *fp = fopen(fname, "w");
    if (fp) {
        fwrite(num, sizeof(long), 2, fp);
        fclose(fp);
    }
}

long GetCardCount(char *fname) {
    FILE *fp;
    long num[2];
    if (!(fp = fopen(fname, "r"))) {
        InitCard(fname);
        return 0;
    }
    fread(num, sizeof(long), 2, fp);
    fclose(fp);
    return num[0];
}

int DumpHtml(char *htmlname) {
    FILE *fp = fopen(htmlname, "w");
    if (!fp) return 0;

    fprintf(fp, "%s\n", comment_card_html_top);
    fprintf(fp, "Mosaic Compiled OS: %s<br>\n", MO_COMMENT_OS);
    fprintf(fp, "<input type=\"hidden\" name=\"os\" value=\"%s\">\n", MO_COMMENT_OS);
    fprintf(fp, "Sysname: %s<br>\n", mo_uname.sysname);
    fprintf(fp, "<input type=\"hidden\" name=\"sysname\" value=\"%s\">\n", mo_uname.sysname);
    fprintf(fp, "Release: %s<br>\n", mo_uname.release);
    fprintf(fp, "<input type=\"hidden\" name=\"release\" value=\"%s\">\n", mo_uname.release);
    fprintf(fp, "%s\n", comment_card_html_bot);

    fclose(fp);
    return 1;
}

void CommentCard(mo_window *win) {
    char *fname, *htmlname, *htmlurl;
    long num[2] = {0};
    int n;

    if (!win) win = mo_next_window(NULL);
    if (!win) return;

    if (!do_comment) {
        if (!(fname = MakeFilename())) return;
        num[0] = GetCardCount(fname);
        num[0]++;
    }

#ifndef PRERELEASE
    if (num[0] == COMMENT_TIME || do_comment) {
        htmlname = tmpnam(NULL);
        if (!htmlname || !DumpHtml(htmlname)) {
            if (!do_comment) free(fname);
            return;
        }

        htmlurl = (char *)calloc(strlen(htmlname) + strlen("file://localhost") + 1, sizeof(char));
        sprintf(htmlurl, "file://localhost%s", htmlname);
        mo_open_another_window(win, htmlurl, NULL, NULL);
        free(htmlurl);
    }
#endif

    if (!do_comment) {
        PutCardCount(num, fname);
        free(fname);
    }
}
