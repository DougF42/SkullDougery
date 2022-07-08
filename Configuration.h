#ifndef C_O_N_F_I_G_U_R_A_T_I_O_N___H
#define C_O_N_F_I_G_U_R_A_T_I_O_N___H

class Config
{
 public:
  const static char *CLASSIFIER_NAME;
  const static char *CLASSIFIER_DIR;
  const static char *PARAM_FILE_NAME;
  const static char *SKULLDOUGERY_ADDR;
  const static int  SKULLDOUGERY_PORT;
  // Input can be from a file OR from the video device
  const static int  INPUT_IMAGE;  // If video device
  //const static char *INPUT_IMAGE; // If file stream;
  
};
#endif
