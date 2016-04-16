#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <sys/stat.h>

int CROSS_VALIDATION = 10;

void is_directory(char *directory){
  struct stat statbuf;

  if(stat(directory, &statbuf) == -1) {
    errx(-1, "stat on directory error, errno = %d, %s", errno, strerror(errno));
  }

  if(!S_ISDIR(statbuf.st_mode)){
    errx(-1, "First parameter must be a directory");
  }
}

//have to send the index because of realloc
int read_file(char ***file, int index, FILE *fp){
  char buffer[200] = {'\0'}, c;
  int i = 0, j = 0;

  while ((c = fgetc(fp)) != EOF){
    if(((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z'))){
      buffer[j++] = c;
    }

    if(j > 0 && (c == ' ' || c == '\n' || c == '.')){
      //add a new word, the next word
      if((file[index] = realloc(file[index], (i + 1) * sizeof(char *))) == NULL) {
        errx(-1, "second level malloc error, errno = %d, %s", errno, strerror(errno));
      }

      //printf("Adding word '%s' at %d\n", buffer, i);

      //alloc the new word from buffer
      if((file[index][i] = calloc(j + 1, sizeof(char))) == NULL) {
        errx(-1, "third level malloc error, errno = %d, %s", errno, strerror(errno));
      }

      //will it add the \0? No idea
      strncpy(file[index][i], buffer, j);

      //printf("%s %d\n", file[index][i], strlen(file[index][i]));
      memset(buffer, '\0', 200);

      j = 0;
      i++;
    }
  }

  return i;
}

int get_vocabulary(char ***learn_examples, int *words_per_learn_example, char ***vocabulary, int total_learn_examples){
  int i, j, k, found;
  int vocabulary_counter = 0;
  char **vocabulary_temp;

  //realloc later
  if((vocabulary_temp = malloc(sizeof(char *))) == NULL) {
    errx(-1, "error on vocabulary malloc, errno = %d, %s", errno, strerror(errno));
  }

  //O(n^2) I know :-)
  for(i = 0; i < total_learn_examples; i++){
    for(j = 0; j < words_per_learn_example[i]; j++){
      found = 0;

      for(k = 0; k < vocabulary_counter; k++){
        if(strcmp(vocabulary_temp[k], learn_examples[i][j]) == 0){
          found = 1;
          //printf("Found '%s' at %d\n", vocabulary_temp[k], k);
          break;
        }
      }

      if(!found){
        if((vocabulary_temp = realloc(vocabulary_temp, (vocabulary_counter + 1) * sizeof(char *))) == NULL) {
          errx(-1, "second level malloc error, errno = %d, %s", errno, strerror(errno));
        }

        //add the new word from buffer
        if((vocabulary_temp[vocabulary_counter] = calloc(strlen(learn_examples[i][j]) + 1, sizeof(char))) == NULL) {
          errx(-1, "vocabulary malloc error, errno = %d, %s", errno, strerror(errno));
        }

        strncpy(vocabulary_temp[vocabulary_counter], learn_examples[i][j], strlen(learn_examples[i][j]));
        //printf("Adding '%s' at %d\n", learn_examples[i][j], vocabulary_counter);

        vocabulary_counter++;
      }
    }
  }

  *vocabulary = vocabulary_temp;
  return vocabulary_counter;
}

void read_examples(char ***examples, char *directory, int *words_per_learn_example, int cross_learning_start_index, int start_index, int end_index, int total_examples){
  char file_path[strlen(directory) + 11]; // will append "/24999.txt" + 1 for \0
  int i, half;
  FILE *fp;
  half = ceil(total_examples / 2);

  for(i = 0; i < total_examples; i++){
    int file_index = ((i >= half ? i % half : i) + cross_learning_start_index) % (end_index + 1);
    snprintf(file_path, strlen(directory) + 15, "%s/%s/%d.txt", directory, i >= half ? "pos" : "neg", file_index);

    //printf("%s\n", file_path);

    if((fp = fopen(file_path, "r")) == NULL) {
      errx(-1, "File opening error, errno = %d, %s - %s", errno, strerror(errno), file_path);
    }

    if((examples[i] = malloc(sizeof(char *))) == NULL) {
      errx(-1, "Second level malloc error, errno = %d, %s", errno, strerror(errno));
    }

    words_per_learn_example[i] = read_file(examples, i, fp);

    //printf("%d %d\n", i, words_per_learn_example[i]);

    fclose(fp);
  }
}

void free_variables(char ***learn_examples, char ***test_examples, char **vocabulary, int *words_per_learn_example,
                    int *words_per_test_example, int total_learn_examples, int total_test_examples, int vocabulary_length){
  int i, j;

  for(i = 0; i < total_learn_examples; i++){
    for(j = 0; j < words_per_learn_example[i]; j++){
      free(learn_examples[i][j]);
    }

    free(learn_examples[i]);
  }

  for(i = 0; i < total_test_examples; i++){
    for(j = 0; j < words_per_test_example[i]; j++){
      free(test_examples[i][j]);
    }

    free(test_examples[i]);
  }

  for(i = 0; i < vocabulary_length; i++){
    free(vocabulary[i]);
  }

  free(learn_examples);
  free(test_examples);
  free(vocabulary);
  free(words_per_learn_example);
  free(words_per_test_example);
}

int main(int argc, char **argv){
  int *words_per_learn_example, *words_per_test_example, *text_neg, *text_pos;
  int i, j, k, l, m, n;
  char *directory;
  char **vocabulary;

  //first level: multiple files
  //second level: multiple words
  //third level: multiple letters
  char ***learn_examples;
  char ***test_examples;

  if(argc != 4){
    errx(-1, "Expecting 3 parameters: <IMDB directory> <start_index> <end_index>");
  }

  int start_index = strtoull(argv[2], NULL, 10);
  int end_index = strtoull(argv[3], NULL, 10);

  if((directory = malloc((1 + strlen(argv[1])) * sizeof(char))) == NULL) {
    errx(-1, "error on directory malloc, errno = %d, %s", errno, strerror(errno));
  }

  memset(directory, '\0', strlen(argv[1]) + 1); //make the string null terminated
  strncpy(directory, argv[1], strlen(argv[1]));

  is_directory(directory);
  
  if(end_index - start_index <= 0){
    errx(-1, "End index has to be larger than start index");
  }

  if((1 + end_index - start_index) % CROSS_VALIDATION != 0){
    errx(-1, "Total number of files (got %d) must be multiple of %d\n", 1 + end_index - start_index, CROSS_VALIDATION);
  }

  int cross_threshold = ((1 + end_index - start_index) / CROSS_VALIDATION);
  int total_test_examples = cross_threshold * 2; //number of files on each cross validation subset
  int total_learn_examples = ((1 + end_index - start_index) * 2) - total_test_examples; //*2 because we have neg and pos subfolders

  printf("Reading from: %s\n", directory);
  printf("Start index: %d\n", start_index);
  printf("End index: %d\n", end_index);
  printf("Total examples to learn: %d\n", total_learn_examples);
  printf("Total examples to test: %d\n\n", total_test_examples);
  //printf("Cross validation (with both negative and positive files) details:\n");

  int true_positive_total = 0;
  int true_negative_total = 0;
  int false_positive_total = 0;
  int false_negative_total = 0;

  int *true_positive, *true_negative, *false_positive, *false_negative;

  if((true_positive = calloc(CROSS_VALIDATION, sizeof(int))) == NULL) {
    errx(-1, "error on true_positive malloc, errno = %d, %s", errno, strerror(errno));
  }

  if((true_negative = calloc(CROSS_VALIDATION, sizeof(int))) == NULL) {
    errx(-1, "error on true_negative malloc, errno = %d, %s", errno, strerror(errno));
  }

  if((false_positive = calloc(CROSS_VALIDATION, sizeof(int))) == NULL) {
    errx(-1, "error on false_positive malloc, errno = %d, %s", errno, strerror(errno));
  }

  if((false_negative = calloc(CROSS_VALIDATION, sizeof(int))) == NULL) {
    errx(-1, "error on false_negative malloc, errno = %d, %s", errno, strerror(errno));
  }

  for(i = 0; i < CROSS_VALIDATION; i++){
    int cross_learning_start_index = cross_threshold * i + start_index;
    int cross_learning_end_index = cross_threshold * ((CROSS_VALIDATION - 1 + i) % (CROSS_VALIDATION)) - 1 + start_index;
    int cross_testing_start_index = cross_learning_end_index + 1;
    int cross_testing_end_index = cross_testing_start_index + cross_threshold - 1;

    if(cross_learning_end_index == -1){
      cross_learning_end_index = end_index;
    }

    //printf("Cross validation number %d, training with files %d.txt - %d.txt, testing with files %d.txt - %d.txt\n", i,
    //  cross_learning_start_index, cross_learning_end_index, cross_testing_start_index, cross_testing_end_index);

    //docs can't be larger then the total number of examples
    if((text_neg = calloc(total_learn_examples, sizeof(int))) == NULL) {
      errx(-1, "error on text_neg malloc, errno = %d, %s", errno, strerror(errno));
    }

    if((text_pos = calloc(total_learn_examples, sizeof(int))) == NULL) {
      errx(-1, "error on text_pos malloc, errno = %d, %s", errno, strerror(errno));
    }

    if((words_per_learn_example = calloc(total_learn_examples, sizeof(int))) == NULL) {
      errx(-1, "error on words_per_learn_example malloc, errno = %d, %s", errno, strerror(errno));
    }

    if((words_per_test_example = calloc(total_learn_examples, sizeof(int))) == NULL) {
      errx(-1, "error on words_per_test_example malloc, errno = %d, %s", errno, strerror(errno));
    }

    //first level malloc for learn_examples
    if((learn_examples = malloc(total_learn_examples * sizeof(char **))) == NULL) {
      errx(-1, "error on first level learn_examples malloc, errno = %d, %s", errno, strerror(errno));
    }

    //first level malloc for test_examples
    if((test_examples = malloc(total_test_examples * sizeof(char **))) == NULL) {
      errx(-1, "error on first level test_examples malloc, errno = %d, %s", errno, strerror(errno));
    }

    //1. Collect all words and other tokens that occur in Examples:
    read_examples(learn_examples, directory, words_per_learn_example, cross_learning_start_index, start_index, end_index, total_learn_examples);

    //Vocabulary ← all distinct words and other tokens in Examples
    int vocabulary_length = get_vocabulary(learn_examples, words_per_learn_example, &vocabulary, total_learn_examples);

    read_examples(test_examples, directory, words_per_test_example, cross_testing_start_index, start_index, end_index, total_test_examples);

    int N_pos = 0;
    int N_neg = 0;

    int learn_half_index = ceil(total_learn_examples / 2);
    int test_half_index = ceil(total_test_examples / 2);

    //n ← total number of words in Texti (counting duplicate words multiple times)
    //get total positive and negative words
    for(j = 0; j < total_learn_examples; j++){
      int index = (i >= learn_half_index ? i % learn_half_index : i);

      if(j >= learn_half_index){
        N_pos += words_per_learn_example[j];
      }
      else{
        N_neg += words_per_learn_example[j];
      }
    }

    double *P_conditional_pos;
    double *P_conditional_neg;

    if((P_conditional_pos = calloc(vocabulary_length, sizeof(double))) == NULL) {
      errx(-1, "error on P_conditional_pos malloc, errno = %d, %s", errno, strerror(errno));
    }

    if((P_conditional_neg = calloc(vocabulary_length, sizeof(double))) == NULL) {
      errx(-1, "error on P_conditional_neg malloc, errno = %d, %s", errno, strerror(errno));
    }

    //for each word Wj in Vocabulary
    for(j = 0; j < vocabulary_length; j++){
      //nj ← number of times word Wj occurs in Texti
      int n_pos = 0;
      int n_neg = 0;

      for(k = 0; k < total_learn_examples; k++){
        for(l = 0; l < words_per_learn_example[k]; l++){
          if(strcmp(vocabulary[j], learn_examples[k][l]) == 0){
            if(k >= learn_half_index){
              n_pos++;
            }
            else{
              n_neg++;
            }
          }
        }
      }

      //P(Wj|Bi) ← (nj + 1) / (n + |Vocabulary|) using logarithm
      P_conditional_pos[j] = log((n_pos + 1) / (double) (N_pos + vocabulary_length));
      P_conditional_neg[j] = log((n_neg + 1) / (double) (N_neg + vocabulary_length));
    }

    
    //CLASSIFY_NAIVE_BAYES_TEXT(Doc)
    for(j = 0; j < total_test_examples; j++){
      double NB_pos = 0;
      double NB_neg = 0;

      for(k = 0; k < words_per_test_example[j]; k++){
        for(l = 0; l < vocabulary_length; l++){
          //all word positions in Doc that contain tokens found in Vocabulary
          if(strcmp(vocabulary[l], test_examples[j][k]) == 0){
            NB_pos += P_conditional_pos[l];
            NB_neg += P_conditional_neg[l];
          }
        }
      }

      if(NB_pos > NB_neg){
        if(j >= test_half_index){
          true_positive[i]++;
        }
        else{
          false_positive[i]++;
        }
      }
      else{
        if(j >= test_half_index){
          false_negative[i]++;
        }
        else{
          true_negative[i]++;
        }
      }
    }

    //printf("TP: %d, FN: %d, TN: %d, FP %d, Total: %d\n", true_positive[i], false_negative[i], true_negative[i], false_positive[i], true_positive[i] + false_negative[i] + true_negative[i] + false_positive[i]);

    true_positive_total += true_positive[i];
    true_negative_total += true_negative[i];
    false_positive_total += false_positive[i];
    false_negative_total += false_negative[i];

    free_variables(learn_examples, test_examples, vocabulary, words_per_learn_example, words_per_test_example, total_learn_examples, total_test_examples, vocabulary_length);
  }

  float true_positive_mean = true_positive_total / (float) CROSS_VALIDATION;
  float true_negative_mean = true_negative_total / (float) CROSS_VALIDATION;
  float false_positive_mean = false_positive_total / (float) CROSS_VALIDATION;
  float false_negative_mean = false_negative_total / (float) CROSS_VALIDATION;

  float true_positive_sd = 0.0;
  float true_negative_sd = 0.0;
  float false_positive_sd = 0.0;
  float false_negative_sd = 0.0;

  for(i = 0; i < CROSS_VALIDATION; i++){
     true_positive_sd += pow(true_positive[i] - true_positive_mean, 2);
     true_negative_sd += pow(true_negative[i] - true_negative_mean, 2);
     false_positive_sd += pow(false_positive[i] - false_positive_mean, 2);
     false_negative_sd += pow(false_negative[i] - false_negative_mean, 2);
  }

  printf("Mean True Positive: %.3f, Standard Deviation: %.3f\n", true_positive_mean, sqrt(true_positive_sd/(float)(CROSS_VALIDATION - 1)));
  printf("Mean True Negative: %.3f, Standard Deviation: %.3f\n", true_negative_mean, sqrt(true_negative_sd/(float)(CROSS_VALIDATION - 1)));
  printf("Mean False Positive: %.3f, Standard Deviation: %.3f\n", false_positive_mean, sqrt(false_positive_sd/(float)(CROSS_VALIDATION - 1)));
  printf("Mean False Negative: %.3f, Standard Deviation: %.3f\n\n", false_negative_mean, sqrt(false_negative_sd/(float)(CROSS_VALIDATION - 1)));

  float recall = true_positive_total / (float) (true_positive_total + false_negative_total);
  float precision = true_positive_total / (float) (true_positive_total + false_positive_total);

  printf("True Positive Rate: %.3f\n", recall);
  printf("False Positive Rate: %.3f\n", false_positive_total / (float) (false_positive_total + true_negative_total));
  printf("F-Measure: %.3f\n", (2 * recall * precision) / (recall + precision));
}
