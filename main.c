#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <sys/stat.h>
#include <stdint.h>
#include <pthread.h>

int CROSS_VALIDATION = 10;
int *true_positive, *true_negative, *false_positive, *false_negative;
int start_index, end_index, cross_threshold, total_test_examples, total_learn_examples;

char *directory;

//from http://www.cse.yorku.ca/~oz/hash.html
unsigned long hash(unsigned char *str){
  unsigned long hash = 5381;
  int c;

  while (c = *str++){
    hash = ((hash << 5) + hash) + c;
  }

  return hash;
}

int compare_hashes(const void *a, const void *b){
  if(*(unsigned long*)a < *(unsigned long*)b){
    return -1;
  }

  if(*(unsigned long*)a > *(unsigned long*)b){
    return 1;
  }

  if(*(unsigned long*)a == *(unsigned long*)b){
    return 0;
  }
}

int binary_search(unsigned long key, unsigned long *array, int array_size, int low, int high){
  int mid = (high + low) / 2;

  if (array[mid] == key){
    int total = 0, i;

    for(i = mid; i >= 0 && array[i] == key; i--){
      total++;
    }

    for(i = mid; i < array_size && array[i] == key; i++){
      total++;
    }

    //this doesn't look very good
    return total - 1;
  }

  if(low < high){
    if(array[mid] > key){
      return binary_search(key, array, array_size, low, mid);
    }
    else{ 
      return binary_search(key, array, array_size, mid + 1, high);
    }
  }

  return 0;
}

void is_directory(){
  struct stat statbuf;

  if(stat(directory, &statbuf) == -1) {
    errx(-1, "stat on directory error, errno = %d, %s", errno, strerror(errno));
  }

  if(!S_ISDIR(statbuf.st_mode)){
    errx(-1, "First parameter must be a directory");
  }
}

int read_file(unsigned long **file, int index, int *each_file_index, int file_index, FILE *fp){
  int i = 0, j = 0;
  char buffer[200] = {'\0'}, c;
  unsigned long *file_temp;

  file_temp = *file;

  while ((c = fgetc(fp)) != EOF){
    if(((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z'))){
      buffer[i++] = tolower(c);
    }

    if(i > 0 && (c == ' ' || c == '\n' || c == '.')){
      //let's ignore words as: a, an, is, in, at, of, no, to, by, on
      if(i < 3){
        memset(buffer, '\0', 200);
        i = 0;
        continue;
      }

      //printf("%s %d\n", buffer, i);

      //add a new word, the next word
      if((file_temp = realloc(file_temp, ++index * sizeof(unsigned long))) == NULL) {
        errx(-1, "file realloc error, errno = %d, %s", errno, strerror(errno));
      }

      file_temp[index - 1] = hash(buffer);
      memset(buffer, '\0', 200);

      i = 0;
      j++;
    }
  }

  if(each_file_index != NULL){
    each_file_index[file_index] = index;
  }

  *file = file_temp;

  return index;
}


int get_vocabulary(unsigned long *examples, int words_examples, unsigned long **vocabulary, int vocabulary_index){
  int i, j, found;
  unsigned long *vocabulary_temp;

  vocabulary_temp = *vocabulary;

  for(i = 0; i < words_examples; i++){
    found = 0;

    for(j = 0; j < vocabulary_index; j++){
      if(vocabulary_temp[j] == examples[i]){
        found = 1;
        break;
      }
    }

    if(!found){
      if((vocabulary_temp = realloc(vocabulary_temp, ++vocabulary_index * sizeof(unsigned long))) == NULL) {
        errx(-1, "second level malloc error, errno = %d, %s", errno, strerror(errno));
      }

      vocabulary_temp[vocabulary_index - 1] = examples[i];
    }
  }

  *vocabulary = vocabulary_temp;
  return vocabulary_index;
}

int read_examples(int words_index, int file_index, char *type, unsigned long **examples, int words_example, int *each_file_index){
  char file_path[strlen(directory) + 11]; // will append "/24999.txt" + 1 for \0
  FILE *fp;

  snprintf(file_path, strlen(directory) + 15, "%s/%s/%d.txt", directory, type, file_index);

  if((fp = fopen(file_path, "r")) == NULL) {
    errx(-1, "File opening error, errno = %d, %s - %s", errno, strerror(errno), file_path);
  }

  words_example = read_file(examples, words_example, each_file_index, words_index, fp);

  fclose(fp);

  return words_example;
}

void *thread(void *arg){
  uint i = (intptr_t)arg;
  int j, k, l;
  int words_pos_learn = 0, words_neg_learn = 0, words_pos_test = 0, words_neg_test = 0;
  int vocabulary_length = 0;

  unsigned long *vocabulary;
  unsigned long *learn_pos, *learn_neg;
  unsigned long *test_pos, *test_neg;

  int cross_learning_start_index = cross_threshold * i + start_index;
  int cross_learning_end_index = cross_threshold * ((CROSS_VALIDATION - 1 + i) % (CROSS_VALIDATION)) - 1 + start_index;
  int cross_testing_start_index = cross_learning_end_index + 1;
  int cross_testing_end_index = cross_testing_start_index + cross_threshold - 1;

  if(cross_learning_end_index == -1){
    cross_learning_end_index = end_index;
  }

  if((learn_pos = malloc(sizeof(learn_pos))) == NULL) {
    errx(-1, "error on learn_pos malloc, errno = %d, %s", errno, strerror(errno));
  }

  if((learn_neg = malloc(sizeof(learn_neg))) == NULL) {
    errx(-1, "error on learn_neg malloc, errno = %d, %s", errno, strerror(errno));
  }

  if((test_pos = malloc(sizeof(test_pos))) == NULL) {
    errx(-1, "error on test_pos malloc, errno = %d, %s", errno, strerror(errno));
  }

  if((test_neg = malloc(sizeof(test_neg))) == NULL) {
    errx(-1, "error on test_neg malloc, errno = %d, %s", errno, strerror(errno));
  }

  //1. Collect all words and other tokens that occur in Examples:
  for(j = 0; j < total_learn_examples; j++){
    int file_index = (j + cross_learning_start_index) % (end_index + 1);

    words_pos_learn = read_examples(j, file_index, "pos", &learn_pos, words_pos_learn, NULL);
    words_neg_learn = read_examples(j, file_index, "neg", &learn_neg, words_neg_learn, NULL);
  }

  qsort(learn_pos, words_pos_learn, sizeof(unsigned long), compare_hashes);
  qsort(learn_neg, words_neg_learn, sizeof(unsigned long), compare_hashes);

  //realloc later
  if((vocabulary = malloc(sizeof(vocabulary))) == NULL) {
    errx(-1, "error on vocabulary malloc, errno = %d, %s", errno, strerror(errno));
  }

  //Vocabulary ← all distinct words and other tokens in Examples
  vocabulary_length += get_vocabulary(learn_pos, words_pos_learn, &vocabulary, 0);
  vocabulary_length = get_vocabulary(learn_neg, words_neg_learn, &vocabulary, vocabulary_length);

  int *each_test_pos_index, *each_test_neg_index;

  if((each_test_pos_index = calloc(total_test_examples, sizeof(each_test_pos_index))) == NULL) {
    errx(-1, "error on each_test_pos_index calloc, errno = %d, %s", errno, strerror(errno));
  }

  if((each_test_neg_index = calloc(total_test_examples, sizeof(each_test_neg_index))) == NULL) {
    errx(-1, "error on each_test_neg_index calloc, errno = %d, %s", errno, strerror(errno));
  }

  for(j = 0; j < total_test_examples; j++){
    int file_index = (j + cross_testing_start_index) % (end_index + 1);

    words_pos_test = read_examples(j, file_index, "pos", &test_pos, words_pos_test, each_test_pos_index);
    words_neg_test = read_examples(j, file_index, "neg", &test_neg, words_neg_test, each_test_neg_index);
  }

  double *P_conditional_pos;
  double *P_conditional_neg;

  if((P_conditional_pos = calloc(vocabulary_length, sizeof(P_conditional_pos))) == NULL) {
    errx(-1, "error on P_conditional_pos calloc, errno = %d, %s", errno, strerror(errno));
  }

  if((P_conditional_neg = calloc(vocabulary_length, sizeof(P_conditional_neg))) == NULL) {
    errx(-1, "error on P_conditional_neg calloc, errno = %d, %s", errno, strerror(errno));
  }

  double words_pos_test_vocabulary_length = words_pos_test + vocabulary_length;
  double words_neg_test_vocabulary_length = words_neg_test + vocabulary_length;

  //for each word Wj in Vocabulary
  for(j = 0; j < vocabulary_length; j++){
    //nj ← number of times word Wj occurs in Texti
    int n_pos = binary_search(vocabulary[j], learn_pos, words_pos_learn - 1, 0, words_pos_learn - 1);
    int n_neg = binary_search(vocabulary[j], learn_neg, words_neg_learn - 1, 0, words_neg_learn - 1);

    //P(Wj|Bi) ← (nj + 1) / (n + |Vocabulary|) using logarithm
    P_conditional_pos[j] = log((n_pos + 1) / words_pos_test_vocabulary_length);
    P_conditional_neg[j] = log((n_neg + 1) / words_neg_test_vocabulary_length);
  }

  k = 0;
  double NB_pos = 0;
  double NB_neg = 0;

  for(j = 0; j < words_pos_test; j++){
    for(l = 0; l < vocabulary_length; l++){
      //all word positions in Doc that contain tokens found in Vocabulary
      //can't use binary search here because we need the indexes (we can but it would be too much work)
      if(vocabulary[l] == test_pos[j]){
        NB_pos += P_conditional_pos[l];
        NB_neg += P_conditional_neg[l];
      }

      //terrible way to test if we are in the last position of the nested loops
      if(j > each_test_pos_index[k] || (j == (words_pos_test - 1) && l == (vocabulary_length - 1))){
        if(NB_pos > NB_neg){
          true_positive[i]++;
        }
        else{
          false_negative[i]++;
        }

        NB_pos = NB_neg = 0;
        k++;
      }
    }
  }

  k = 0;
  NB_pos = 0;
  NB_neg = 0;

  for(j = 0; j < words_neg_test; j++){
    for(l = 0; l < vocabulary_length; l++){
      //all word positions in Doc that contain tokens found in Vocabulary
      //can't use binary search because test can't be ordered (unless we put each file's words in a separated array)
      if(vocabulary[l] == test_neg[j]){
        NB_pos += P_conditional_pos[l];
        NB_neg += P_conditional_neg[l];
      }

      //we need this ugly logic because all test words are in one array
      //terrible way to test if we are in the last position of the nested loops
      if(j > each_test_neg_index[k] || (j == (words_neg_test - 1) && l == (vocabulary_length - 1))){
        if(NB_pos > NB_neg){
          false_positive[i]++;
        }
        else{
          true_negative[i]++;
        }

        NB_pos = NB_neg = 0;
        k++;
      }
    }
  }
}

int main(int argc, char **argv){
  int i;

  if(argc != 4){
    errx(-1, "Expecting 3 parameters: <IMDB directory> <start_index> <end_index>");
  }

  start_index = strtoull(argv[2], NULL, 10);
  end_index = strtoull(argv[3], NULL, 10);

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

  cross_threshold = ((1 + end_index - start_index) / CROSS_VALIDATION);
  total_test_examples = cross_threshold; //number of files on each cross validation subset
  total_learn_examples = ((1 + end_index - start_index)) - total_test_examples; //*2 because we have neg and pos subfolders

  printf("Reading from: %s\n", directory);
  printf("Start index: %d\n", start_index);
  printf("End index: %d\n", end_index);
  printf("Total examples to learn: %d\n", total_learn_examples);
  printf("Total examples to test: %d\n\n", total_test_examples);

  int true_positive_total = 0;
  int true_negative_total = 0;
  int false_positive_total = 0;
  int false_negative_total = 0;

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

  //10 threads, I know
  pthread_t threads[CROSS_VALIDATION];

  for(i = 0; i < CROSS_VALIDATION; i++){
    if (pthread_create(&(threads[i]), NULL, &thread, (void *)(intptr_t)i) != 0){
      errx(-1, "can't create thread :[%s]");
    }
  }

  for(i = 0; i < CROSS_VALIDATION; i++){
    if(pthread_join(threads[i], NULL) != 0){
      errx(-1, "pthread_join %s");
    }
  }

  for(i = 0; i < CROSS_VALIDATION; i++){
    true_positive_total += true_positive[i];
    true_negative_total += true_negative[i];
    false_positive_total += false_positive[i];
    false_negative_total += false_negative[i];
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

  printf("Mean True Positive: %.3f (total %d), Standard Deviation: %.3f\n", true_positive_mean, true_positive_total, sqrt(true_positive_sd/(float)(CROSS_VALIDATION - 1)));
  printf("Mean True Negative: %.3f (total %d), Standard Deviation: %.3f\n", true_negative_mean, true_negative_total, sqrt(true_negative_sd/(float)(CROSS_VALIDATION - 1)));
  printf("Mean False Positive: %.3f (total %d), Standard Deviation: %.3f\n", false_positive_mean, false_positive_total, sqrt(false_positive_sd/(float)(CROSS_VALIDATION - 1)));
  printf("Mean False Negative: %.3f (total %d), Standard Deviation: %.3f\n\n", false_negative_mean, false_negative_total, sqrt(false_negative_sd/(float)(CROSS_VALIDATION - 1)));

  float recall = true_positive_total / (float) (true_positive_total + false_negative_total);
  float precision = true_positive_total / (float) (true_positive_total + false_positive_total);

  printf("Precision: %.3f\n", precision);
  printf("Recall: %.3f\n", recall);
  printf("False Positive Rate: %.3f\n", false_positive_total / (float) (false_positive_total + true_negative_total));
  printf("F-Measure: %.3f\n", (2 * recall * precision) / (recall + precision));
}
