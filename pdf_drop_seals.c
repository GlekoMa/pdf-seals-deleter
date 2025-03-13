#include <windows.h>
#include "pdf_drop_seals.h"
#include "mupdf/pdf/object.h"
#include "mupdf/pdf/page.h"
#include "mupdf/pdf/xref.h"

// Define A4 size
#define A4_WIDTH 595.0f
#define A4_HEIGHT 842.0f

// NOTE: we need to realize this
int remove_duplicates(int* arr, int n)
{
    int* current;
    int* end = arr + n - 1;

    for (current = arr + 1; arr < end; ++arr, current = arr + 1) {
        while (current <= end) {
            if (*current == *arr) {
                *current = *end--;
                --n;
            } else {
                current++;
            }
        }
    }
    return n;
}

int get_page_xref_arr(fz_context* ctx, pdf_document* doc, int page_number, int xref_arr[], int at)
{
    pdf_obj* pageref = pdf_lookup_page_obj(ctx, doc, page_number);
    pdf_obj* rsrc = pdf_dict_get_inheritable(ctx, pageref, PDF_NAME(Resources));
    pdf_obj* xobj = pdf_dict_get(ctx, rsrc, PDF_NAME(XObject));

    int n = pdf_dict_len(ctx, xobj);
    for (int i = 0; i < n; ++i) {
        pdf_obj* imagedict = pdf_dict_get_val(ctx, xobj, i);
        xref_arr[at + i] = pdf_to_num(ctx, imagedict);
    }
    return n;
}

int get_doc_xref_arr(fz_context* ctx, pdf_document* doc, int xref_arr[])
{
    int page_count = pdf_count_pages(ctx, doc);
    int n = 0;
    for (int pno = 0; pno < page_count; ++pno) {
        n += get_page_xref_arr(ctx, doc, pno, xref_arr, n);
    }
    return n;
}

// TODO: Should we reset the page refs after inserting?
pdf_obj* new_page(fz_context* ctx, pdf_document* doc, fz_rect mediabox)
{
    fz_buffer* contents = fz_new_buffer(ctx, 0); // capacity = 0
    pdf_obj* resources = pdf_add_new_dict(ctx, doc, 1); // initial = 1
    
    // Force size to A4
    fz_rect a4_mediabox = { 0, 0, A4_WIDTH, A4_HEIGHT };
    pdf_obj* page_obj = pdf_add_page(ctx, doc, a4_mediabox, 0, resources, contents); // rotate = 0
    pdf_insert_page(ctx, doc, -1, page_obj); // pno = -1
    return page_obj;
}

void insert_image(fz_context* ctx, pdf_document* doc, fz_image* img, pdf_obj* page_obj, const char* img_name)
{
    pdf_obj* ref_new = pdf_add_image(ctx, doc, img);

    pdf_obj* resources = pdf_dict_get_inheritable(ctx, page_obj, PDF_NAME(Resources));
    pdf_obj* xobject = pdf_dict_put_dict(ctx, resources, PDF_NAME(XObject), 2); // initial = 2
    pdf_dict_puts(ctx, xobject, img_name, ref_new);

    fz_buffer* nres = fz_new_buffer(ctx, 50); // capacity = 50

    // Calculate scale for A4
    float scale_w = A4_WIDTH / img->w;
    float scale_h = A4_HEIGHT / img->h;
    float scale = fz_min(scale_w, scale_h);
    
    // Calculate offset
    float offset_x = (A4_WIDTH - img->w * scale) / 2;
    float offset_y = (A4_HEIGHT - img->h * scale) / 2;
    
    fz_append_printf(ctx, nres, "q\n%g 0 0 %g %g %g cm\n/%s Do\nQ\n", 
        img->w * scale, img->h * scale, offset_x, offset_y, img_name);

    pdf_obj* new_obj = NULL;
    pdf_obj* newconts = pdf_add_stream(ctx, doc, nres, new_obj, 0); // compressed = 0
    pdf_obj* carr = pdf_new_array(ctx, doc, 5); // initialcap = 5
    pdf_array_push(ctx, carr, newconts);
    pdf_dict_put(ctx, page_obj, PDF_NAME(Contents), carr);
}

void extract_and_add_right_image_as_new_page(fz_context* ctx, pdf_document* doc, pdf_document* doc_new, int xref)
{
    // Get the image from original pdf based xref
    pdf_obj* ref = pdf_new_indirect(ctx, doc, xref, 0); // gen = 0
    fz_image* img = pdf_load_image(ctx, doc, ref);

    // Judge whether the image is a seal
    if (img->w > img->h) {
        return;
    }

    // Create a dynamic image name
    char img_name[20];
    snprintf(img_name, sizeof(img_name), "Img%d", xref);

    // Create a new page & insert the image
    fz_rect mediabox = { 0, 0, img->w, img->h };
    pdf_obj* page_obj = new_page(ctx, doc_new, mediabox);
    insert_image(ctx, doc_new, img, page_obj, img_name);
}

void pdf_drop_seals(char* input, char* output)
{
    // Initialize MuPDF context and open document
    fz_context* ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
    fz_register_document_handlers(ctx);
    pdf_document* doc = pdf_open_document(ctx, input);

    // Get xref set of all images
    int xref_arr[MAX_XREF_LEN];
    int xref_arr_len = get_doc_xref_arr(ctx, doc, xref_arr);
    int unique_xref_arr_len = remove_duplicates(xref_arr, xref_arr_len);

    // Insert all right images to a new pdf & save it
    pdf_document* doc_new = pdf_create_document(ctx);
    for (int i = 0; i < unique_xref_arr_len; ++i) {
        extract_and_add_right_image_as_new_page(ctx, doc, doc_new, xref_arr[i]);
    }
    pdf_save_document(ctx, doc_new, output, NULL); // opts = NULL

    // Clean up resources (ok, we know there are still many others to drop)
    pdf_drop_document(ctx, doc_new);
    pdf_drop_document(ctx, doc);
    fz_drop_context(ctx);
}
