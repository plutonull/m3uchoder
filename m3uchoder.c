/*
 * Copyright (c) 2026 Allon L.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <libavformat/avformat.h>
#include <libavutil/log.h>
#include <nbcompat/stdio.h>
#include <nbcompat/string.h>

#define MTOAVF(x) (x*60*AV_TIME_BASE)

/* Break Options */
#define CHODE_PATH "Chode FM\\"
#define LEN_CHODE_PATH 9

#define CHODE_PFIX "CHODE FM "
#define LEN_CHODE_PFIX 9
#define CHODE_EXT "mp3"
#define LEN_CHODE_EXT 3
#define MAX_CHODE  24

const char *UCHODE_LIST[] = {
	"CHODE FM (G-Man).mp3",
	"Charger.mp3"
};
#define MAX_UCHODE 2


int i_chode, t_chode;
int i_uchode, t_uchode;

void ins_chode(FILE* o_pl, bool uchode);

long long int a_remains;
long long int a_elapsed;

void read_album(FILE* pl, FILE* o_pl);
long long int look_ahead(FILE* pl); 

char *p_album = NULL;
bool is_sep(char* line);

long long int read_len(char* file);

int main(int argc, char** argv) 
{
	
	if (argc < 2) {
		fprintf(stderr, "No playlist provided\n");
		return 1;
	}

	FILE *pl, *o_pl;
	size_t line_l;
	char *line, *fn, *fn2, *prev, *path;
	double m;

	av_log_set_level(AV_LOG_FATAL);
	pl = fopen(argv[1], "r");
	if (pl == NULL) {
		fprintf(stderr, "Error opening %s\n", argv[1]);
		return 1;
	}

	prev = argv[1];
	while ((fn = strchr(prev,'\\')) != NULL) {
		prev = fn+1;
	}

	fn = strdup(prev);
	path = strdup(argv[1]);
	path[strlen(path) - strlen(fn)] = '\0';

	fn2 = (char*)malloc(strlen(path) + strlen(fn) + 1 + 3);
	strcpy(fn2, path);

	prev = strchr(fn, '.');
	if (prev != NULL) {
		*prev = '\0';
		strcat(fn2, fn);
		strcat(fn2, "_CH.");
		strcat(fn2, prev + 1);
	}
	else {
		strcat(fn2, fn);
		fn2 = strcat(fn2, "_CH");
	}


	o_pl = fopen(fn2, "w+");
	free(path);
	free(fn2);
	free(fn);

	a_elapsed = 0;
	a_remains = 0;
	i_chode = 0;
	i_uchode = 0;
	t_chode = 0;
	t_uchode = 0;

	line_l = 0;
	line = NULL;
	getline(&line, &line_l, pl);
	if (strcmp(line, "#EXTM3U\n") != 0) {
		fprintf(stderr, "Not a M3U playlist\n");
		return 1;
	}
	fprintf(o_pl, "%s",line);

	free(line);
	while (!feof(pl)) {
		
		a_remains = look_ahead(pl);

		read_album(pl, o_pl);

		m = ((double)a_elapsed / AV_TIME_BASE) / 60;
		printf("Album Remainder: %d:%f\n", (int)m, fmod(m, 1.0) * 60);
	}
	printf("Inserted CHODES:%d UCHODES:%d\n", t_chode, t_uchode);
	fclose(pl);
	fclose(o_pl);
#ifdef _WIN32 || _WIN64
	system("pause");
#endif
	return 0;
}

/* 
 * Read lines from pl until reaching a seperator, 
 * determine when to insert breaks
 */
void read_album(FILE *pl, FILE *o_pl) {
	bool skip = false;
	size_t line_l, line2_l;
	char *line, *line2;
	long long int l_len, skip_len;
	fpos_t pos;

	skip_len = 0;
	a_elapsed = 0;
	while (!feof(pl)) {
		line_l = 0;
		line = NULL;
		line2_l = 0;
		line2 = NULL;

		char c = (char)fgetc(pl);
		if (c == '\n' || c == EOF)
			continue;
		ungetc(c, pl);

		fgetpos(pl, &pos);

		getline(&line, &line_l, pl);
		getline(&line2, &line2_l, pl);

		line[strlen(line) - 1] = '\0';
		line2[strlen(line2) - 1] = '\0';
		if (is_sep(line2)) {
			
			if (skip && skip_len >= MTOAVF(10)) {
				a_elapsed = skip_len;
				ins_chode(o_pl, false);
			}
			else if (skip_len <= MTOAVF(10)) {
				a_elapsed = skip_len;
				ins_chode(o_pl, true);
			}
			fsetpos(pl, &pos);
			free(line2);
			free(line);
			break;
		}
		fprintf(o_pl, "%s\n%s\n\n", line, line2);
		l_len = read_len(line2);
		if (l_len < 0)
			fprintf(stderr, "Failed to open %s\n", line2);
		a_remains -= l_len;
		if (!skip) {
			a_elapsed += l_len;
			if (a_elapsed >= MTOAVF(15) && a_remains >= MTOAVF(15)) {
				a_elapsed = 0;
				ins_chode(o_pl, false);
			}
			else if(a_remains <= MTOAVF(15)){
				if (a_elapsed >= MTOAVF(10)) {
					ins_chode(o_pl, false);
				}
				else {
					skip_len = a_elapsed;
				}
				skip = true;
				
			}
		}
		else {
			skip_len += l_len;
			if (skip_len >= MTOAVF(15) && a_remains >= MTOAVF(5)) {
				ins_chode(o_pl, false);
				skip_len = 0;
			}
		}
		free(line);
		free(line2);
	}
	return;
}

/*
 * Read lines from pl, Look ahead to the end of the album,
 * Return the duration, or -1 if an error occurs
 */

long long int look_ahead(FILE *pl) 
{
	long long int r_len = 0, l_len;
	size_t line_l;
	char* line, *s_p_album;
	fpos_t pos;

	s_p_album = strdup(p_album);
	fgetpos(pl, &pos);

	while (!feof(pl)) {
		line_l = 0;
		line = NULL;
		getline(&line, &line_l, pl);

		if (feof(pl) || line[0] == '#' || line[0] == '\n') {
			free(line);
			continue;
		}

		line[strlen(line) - 1] = '\0';
		if (is_sep(line))
			break;
		l_len = read_len(line);
		if (l_len < 0)
			return -1;
		r_len += l_len;

		free(line);
	}

	fsetpos(pl, &pos);

	free(p_album);
	p_album = s_p_album;

	return r_len;
}


/*
 * Read length from file,
 * Returns -1 on error opening file,
 * and -2 on error reading info from file
 * return is a long long int of 1/AV_TIME_BASE seconds
 */
long long int read_len(char *file) 
{
	AVFormatContext	*c = NULL;
	long long int d;

	if (avformat_open_input(&c, file, NULL, NULL) < 0)
		return -1;
	if (avformat_find_stream_info(c, NULL) < 0)
		return -2;

	d = c->duration;
	avformat_close_input(&c);
	avformat_free_context(c);
	return d;
}

/*
 * Determine whether file is a seperator,
 * return a boolean value.
 * file must be a path, of at least two folders length
 */

bool is_sep(char *file)
{
	int i;
	char *c_album = NULL;
	int e_pos = 0;
	bool ret = false;

	for (i = (int)strlen(file); i >= 0; i--) {
		if (file[i] == '\\') {
			if (e_pos == 0) {
				e_pos = i;
			}
			else {
				c_album = strndup(file + i, e_pos - i);
				break;
			}
		}
	}

	if (c_album == NULL) {
		fprintf(stderr, "Seperator not found on line \"%s\"\n", file);
		abort();
	}

	if (p_album != NULL) {
		if (memcmp(c_album, p_album, min(strlen(c_album), strlen(p_album))) != 0){
			if (!(memcmp(p_album, "\\D_I_", 5) == 0 && memcmp(c_album, "\\D_I_", 5) == 0))
				ret = true;
			free(p_album);
		}
	}

	p_album = c_album;
	return ret;
}

/*
 * Insert a break into o_pl,
 * a special break is inserted if uchode is true.
 */

void ins_chode(FILE *o_pl, bool uchode) {
	int i;
	char *insert;
	
	if (uchode) {
		t_uchode++;
		if (++i_uchode > MAX_UCHODE) {
			i_uchode = 1;
		}
		i = i_uchode - 1;

		insert = (char*)malloc(LEN_CHODE_PATH + strlen(UCHODE_LIST[i]) + 1);
		sprintf(insert, "%s%s", CHODE_PATH, UCHODE_LIST[i]);
	}
	else {
		int n;
		int ct;

		t_chode++;
		if (++i_chode > MAX_CHODE) {
			i_chode = 1;
		}

		n = i_chode;
		ct = 0;
		while (n > 0) {
			n /= 10;
			ct++;
		}

		insert = (char*)malloc(LEN_CHODE_PATH + LEN_CHODE_PFIX + LEN_CHODE_EXT + ct + 2);
		sprintf(insert, "%s%s%d.%s", CHODE_PATH, CHODE_PFIX, i_chode, CHODE_EXT);
	}

	/* m3u spec says the 0 should be the length of the file, but Windows Media Player seems to ignore that */
	fprintf(o_pl, "#EXTINF:0,%s\n%s\n\n", insert+LEN_CHODE_PATH, insert);
	free(insert);
}
