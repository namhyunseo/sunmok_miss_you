// pintos -v -k -T 60 -m 20   --fs-disk=10 -p tests/userprog/create-normal:create-normal --swap-disk=4 -- -q   -f run create-normal < /dev/null 2> tests/userprog/create-normal.errors > tests/userprog/create-normal.output
/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"

/* project3 spt */
#include "hash.h"
#include "include/vm/uninit.h"
#include "include/vm/anon.h"
#include "include/vm/file.h"
#include "include/threads/mmu.h"

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, struct aux_page *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* 새로운 페이지 할당 */
		struct page *page = malloc(sizeof *page);
		
		uninit_new(page, upage, init, type, aux, NULL);
		page->writable = writable;
		
		switch (type){
		case VM_ANON :
			page->uninit.page_initializer = anon_initializer;
			break;
	
		case VM_FILE :
			page->uninit.page_initializer = file_backed_initializer;
			break;
		
		case VM_ANON | VM_MARKER_0 :
			page->uninit.page_initializer = anon_initializer;
			break;
		default :
			free(page);
			goto err;
		}


		bool succ = spt_insert_page(spt, page);
		if (!succ) {
			free(page);
			goto err;
		}
		return true;
	}else goto err;
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct  supplemental_page_table *spt UNUSED, void *va UNUSED) {
	struct page *page = NULL;
	// va에 대한 검증 필요하려나

	struct page p;
	struct hash_elem *e;
	void *upage = pg_round_down (va);
	p.va = upage;
	e = hash_find(&spt->pages, &p.he);
	if (e == NULL) return NULL;

	page = hash_entry(e, struct page, he);
	
	if (page == NULL) {
		return NULL;
	}

	return page;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	bool succ = false;

	struct hash_elem *old = hash_insert(&spt->pages, &page->he);
	if (old == NULL) { 
		succ = true;
	}
	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc()하고 프레임을 가져온다. 사용 가능한 페이지가 없으면,
 * 페이지를 내쫓고(evict) 그것을 반환한다. 이 함수는 항상 유효한
 * 주소를 반환한다. 즉, 사용자 풀 메모리가 가득 찼다면, 이 함수는
 * 프레임을 내쫓아서 사용할 수 있는 메모리 공간을 확보한다. */
static struct frame *
vm_get_frame (void) {
	void *kva = palloc_get_page(PAL_USER);
	if (!kva) PANIC("todo");
	struct frame *frame = malloc(sizeof *frame);
	frame->kva = kva;
	frame->page = NULL;

	ASSERT (frame != NULL); 
	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	
	/* va가 SPT에 있는지 확인*/
	if(!(page = spt_find_page(spt, addr))) return false;

	/* write access */
	if(write && !page->writable){
		return vm_handle_wp(page);
	}

	/* present */
	bool suc_claim = true;
	if(not_present){
		bool suc_claim = vm_do_claim_page (page);
	}

	/* user에 대한 평가도 진행하긴 해야할 것 같다. 그런데 뭘 해야할지 모르겠음*/

	return suc_claim;
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* 가상 주소에 매핑될 페이지를 확보한다. */
bool
 vm_claim_page (void *va UNUSED) {
	struct page *page = NULL;
	/* spt에서 데이터로 채워야 하는 페이지를 가져온다. */
	page = spt_find_page(&thread_current()->spt, va);
	if(page == NULL) return NULL;

	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	struct thread *curr = thread_current();
	if (!pml4_set_page (curr->pml4, page->va, frame->kva, page->writable)) {
		page->frame = NULL;
		frame->page = NULL;
		palloc_free_page (frame->kva);
		free (frame);
		return false;
	}

	return swap_in (page, frame->kva);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	/* 해쉬 테이블 초기화 */
	hash_init(&spt->pages, spt_hash_func, spt_less_func, NULL);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst,
		struct supplemental_page_table *src) {
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
}

/* va에 대한 hash값을 구해서 반환한다. */
static uint64_t
spt_hash_func (const struct hash_elem *hash_e, void *aux) {
	struct page *page = hash_entry(hash_e, struct page, he);

	/* hash 값 계산 */
	uint64_t hash = hash_bytes(&page->va, sizeof page->va);

	return hash;
}

/* 해쉬 두개의 크기를 비교한다. 보조 테이블 활용 가능*/
static bool
spt_less_func (const struct hash_elem *a,
		const struct hash_elem *b,
		void *aux) {

	struct page *page_a = hash_entry(a, struct page, he);
	struct page *page_b = hash_entry(b, struct page, he);

	return page_a->va > page_b->va;
}
