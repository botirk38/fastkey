#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


Config parse_cli_args(int argc, char **argv) {


  for(int i=1; i < argc; i++ ) {
    printf("argv[%d] = %s\n", i, argv[i]);

  }

  Config config = {config.port = 6379};

  for(int i=1; i < argc; i++ ) {

    if(strcmp(argv[i], "--port") == 0) {
      config.port = atoi(argv[i+1]);
    }
    


  }

  return config;
}
