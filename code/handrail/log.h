#ifndef handrail_log_h_INCLUDED
#define handrail_log_h_INCLUDED

// TODO: Logging modes, log to files, different logging targets.
// TOOD: Don't use stdio, do our own string formatting, etc.

#define LOG_FILE true

#if LOG_FILE
FILE* handrail_static_log_file;
#endif

void log_init();
void log_msg(char* msg);
void log_err(char* msg);
void log_exit(char* msg);

#ifdef CSM_IMPLEMENTATION

void log_init() {
#if LOG_FILE
	char fname[128];
	//sprintf(fname, "log_%u.txt", time(NULL));
	sprintf(fname, "log.txt");
	handrail_static_log_file = fopen(fname, "wb");
#endif
}

void log_msg(char* msg) {
    printf("%s", msg);
#if LOG_FILE
	fprintf(handrail_static_log_file, "%s", msg);
#endif
}

// NOW: figure out if this is firing at all
void log_err(char* msg) {
    fprintf(stderr, "error: %s", msg);
#if LOG_FILE
	fprintf(handrail_static_log_file, "error: %s", msg);
#endif
}

void log_exit(char* msg) {
    log_err(msg);
    exit(1);
}

#endif
#endif
