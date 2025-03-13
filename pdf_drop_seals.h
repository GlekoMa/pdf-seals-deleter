#include <mupdf/fitz.h>
#include <mupdf/pdf/object.h>
#include <mupdf/pdf/page.h>
#include <mupdf/pdf/xref.h>

#define MAX_XREF_LEN 1024

int remove_duplicates(int* arr, int n);
int get_page_xref_arr(fz_context* ctx, pdf_document* doc, int page_number, int xref_arr[], int at);
int get_doc_xref_arr(fz_context* ctx, pdf_document* doc, int xref_arr[]);
pdf_obj* new_page(fz_context* ctx, pdf_document* doc, fz_rect mediabox);
void insert_image(fz_context* ctx, pdf_document* doc, fz_image* img, pdf_obj* page_obj, const char* img_name);
void extract_and_add_right_image_as_new_page(fz_context* ctx, pdf_document* doc, pdf_document* doc_new, int xref);
void pdf_drop_seals(char* input, char* output);
