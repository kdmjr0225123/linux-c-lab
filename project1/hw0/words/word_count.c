#include "word_count.h"

/* Basic utililties */

char *new_string(char *str) {
  return strcpy((char *)malloc(strlen(str)+1), str);
}

void init_words(WordCount **wclist) {
  /* Initialize word count.  */
  *wclist = NULL;
}

size_t len_words(WordCount *wchead) {
    size_t len = 0;
    while (wchead != NULL) {
        len++;
        wchead = wchead->next;
    }
    return len;
}

WordCount *find_word(WordCount *wchead, char *word) {
  /* Return count for word, if it exists */
  while (wchead != NULL) {
      if (strcmp(wchead->word, word) == 0) {
          return wchead;
      }
      wchead = wchead->next;
  }
  return NULL;
}

void add_word(WordCount **wclist, char *word) {
  /* If word is present in word_counts list, increment the count, otw insert with count 1. */
  WordCount *found = find_word(*wclist, word);
  if (found != NULL) {
      found->count++;
      return;
  }

  WordCount *new_node = (WordCount *)malloc(sizeof(WordCount));
  new_node->word = new_string(word);
  new_node->count = 1;
  new_node->next = *wclist;
  *wclist = new_node;
}

void fprint_words(WordCount *wchead, FILE *ofile) {
  /* print word counts to a file */
  WordCount *wc;
  for (wc = wchead; wc; wc = wc->next) {
    fprintf(ofile, "%i\t%s\n", wc->count, wc->word);
  }
}
