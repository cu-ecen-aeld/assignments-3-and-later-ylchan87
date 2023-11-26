#include <stdio.h>
#include <syslog.h>
#include <string.h>

int main( int argc, char *argv[] )  {
    openlog(NULL, 0, LOG_USER);

    if (argc!=3){
        syslog(LOG_ERR, "Invalid Number of arguments: %d", argc);
        return 1;
    }

    const char* filename = argv[1];
    const char* writestr = argv[2];

    FILE* fp = fopen( filename , "w" );
    // printf("Writing %s to %s size %ld", writestr, filename, strlen(writestr));
    syslog(LOG_DEBUG, "Writing %s to %s", writestr, filename);
    fwrite(writestr , sizeof(char) , strlen(writestr) , fp );
    fclose(fp);

;    return 0;
}