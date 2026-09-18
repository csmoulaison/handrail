#ifndef handrail_log_h_INCLUDED
#define handrail_log_h_INCLUDED

// TODO: Logging modes, log to files, different logging targets.
// TOOD: Don't use stdio, do our own string formatting, etc.

void log_msg(String msg);
void log_err(String msg);
void log_exit(String msg);

#ifdef CSM_IMPLEMENTATION

void log_msg(String msg) {
	assert(msg.text[msg.len] == '\0');
    printf("%s", msg.text);
}

void log_err(String msg) {
	assert(msg.text[msg.len] == '\0');
    fprintf(stderr, "error: %s", msg.text);
}

void log_exit(String msg) {
    log_err(msg);
	exit(1);
}

#endif
#endif
