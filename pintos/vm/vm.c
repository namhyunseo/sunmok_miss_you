/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"

/* project3 spt */
#include "hash.h"
#include "threads/mmu.h"

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

/* 초기화 함수를 사용하여 보류 중인 페이지 객체를 생성합니다. 
 페이지를 생성하려면 직접 생성하지 말고 이 함수나 
 `vm_alloc_page`를 통해 생성하세요. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {
	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;
	/* uppage가 이미 사용 중인지 확인하세요. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: 페이지를 생성하고, VM 유형에 따라 초기화 파일을 가져온 다음, 
		uninit_new를 호출하여 "uninit" 페이지 구조체를 생성합니다. 
		uninit_new를 호출한 후 필드를 수정해야 합니다. */

		// 1. 페이지 생성 - palloc으로 할당 하려 하였으나 remove쪽에서 free하기 때문에 malloc으로 변경
		struct page *page = malloc(sizeof(struct page));
		if (page == NULL)
			goto err;
		// 2. uninit 페이지로 초기화 - "uninit" 페이지 구조체를 생성
		uninit_new (page, upage, init, type, aux, NULL);
		page->writable = writable;  // writable 설정

		// 타입별로 page_initializer 설정
		switch (type) { 
			case VM_ANON:
				page->uninit.page_initializer = anon_initializer;
				break;
			case VM_FILE:
				page->uninit.page_initializer = file_backed_initializer;
				break;
			case VM_ANON | VM_MARKER_0:
				page->uninit.page_initializer = anon_initializer;
				break;
			default:
				free(page);
				goto err;
		}
		// 3. spt에 페이지 삽입
		if (spt_insert_page(spt, page)){
			return true;
		}
		else{
			free(page);
			goto err;
		}
	}

err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	struct page *page = NULL;
	// va에 대한 검증 필요하려나
	// 초기화되지 않은 페이지의 va를 찾을 때 문제가 될 수도?
	struct page *p = malloc(sizeof(struct page));
	p->va = pg_round_down(va);
	struct hash_elem *e = hash_find(&spt->pages, &p->he);
	if(e == NULL){
		free(p);
		return NULL;
	}
	
	page = hash_entry(e, struct page, he);

	if (page == NULL) {
		return NULL;
	}
	free(p);
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

/* 한 페이지를 제거하고 해당 프레임을 반환합니다.
 * 오류 발생 시 NULL을 반환합니다.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: 피해자를 교체하고 퇴출된 프레임을 반환합니다. */

	return NULL;
}

/* palloc() 함수를 사용하여 프레임을 가져옵니다. 
 * 사용 가능한 페이지가 없으면 해당 페이지를 제거하고 반환합니다. 
 * 이 함수는 항상 유효한 주소를 반환합니다. 
 * 즉, 사용자 풀 메모리가 가득 차면 이 함수는 프레임을 제거하여 
 * 사용 가능한 메모리 공간을 확보합니다.*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = malloc(sizeof(struct frame));
	if (frame == NULL) {
        PANIC("todo");
    }
	// palloc으로 프레임 할당
	frame->kva = palloc_get_page(PAL_USER);
	
	// 메모리가 가득 찼거나 공간 부족 등으로 실패
	if (frame->kva == NULL) {
		free(frame);
		PANIC("todo"); // swap 처리 필요
		// 1. evict 대상 프레임 선택
		// 2. 해당 프레임을 참조하는 페이지 테이블 항목 제거
		// 3. 필요하면 swap 또는 파일로 기록
	}

	frame->page = NULL;

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
}

/* write_protected 페이지에서 오류를 처리합니다. */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt = &thread_current ()->spt;
	struct page *page = NULL;
	/* 페이지 유효성 검사 */
	// TODO : spt_find_page안에서 pg_round_down을 하는 것이 아닌 여기서만 하는 것이 맞겠다.
	// 1. spt에서 페이지 찾기
	page = spt_find_page(spt,addr);
	// 2. 페이지가 존재하지 않으면 segfault
	if (page == NULL) {
		return false;
	}
	// 3. SPT에 있지만 쓰기 보호인 경우
	// TODO: not_prsent가 정확히 어떤 변수인지 -- PTE 테이블에 없다
	// 단순 존재하지 않는다가 spt에서 존재하지 않는 것인지
	if (write && !not_present && !page->writable) {
		return vm_handle_wp(page);		// 구현 필요
	}
	return vm_do_claim_page (page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* VA에 할당된 페이지를 청구합니다. */
bool
vm_claim_page (void *va UNUSED) {
	struct page *page = NULL;
	// spt에서 va에 해당하는 페이지 찾기
	struct supplemental_page_table *spt = &thread_current ()->spt;
	page = spt_find_page (spt, va);
	if (page == NULL) {
		return false;
	}

	return vm_do_claim_page (page);
}

/* 페이지를 요청하고 mmu를 설정합니다. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();

	/* Set links */
	frame->page = page;  
	page->frame = frame;

	/* TODO: 페이지의 VA를 프레임의 PA에 매핑하기 위해 페이지 테이블 항목을 삽입합니다. */
	// Frame 테이블에 삽입 -> pml4에 매핑
	struct thread *curr = thread_current();
	// pml4에 매핑(= 메모리 할당 성공)
	if(!pml4_set_page (curr->pml4, page->va, frame->kva, page->writable)){
		return false;
	}
	// pml4_get_page (curr->pml4, page->va) == frame->kva
	if(pml4_get_page (curr->pml4, page->va) == NULL){
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

/* src에서 dst로 보충 페이지 테이블 복사 */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst,
		struct supplemental_page_table *src) {
	// 해시 테이블을 순회하는 i
	struct hash_iterator i;
	// i와 src의 해시 테이블 연결?
	hash_first (&i, &src->pages);
	
	while (hash_next (&i))
	{
		// 하나의 페이지를 가져옴
		struct page *p = hash_entry (hash_cur (&i), struct page, he);
		if(page_get_type(p) == VM_ANON){
			if(!vm_alloc_page_with_initializer(VM_ANON, p->va, p->writable,
				p->uninit.init, p->uninit.aux))
				{
					return false;
				}
			if(!vm_claim_page(p->va)){
				return false;
			}
		}
		else if(page_get_type(p) == VM_FILE){
			if(!vm_alloc_page_with_initializer(VM_FILE, p->va, p->writable,
				p->uninit.init, p->uninit.aux))
				{
					return false;
				}
			if(!vm_claim_page(p->va)){
				return false;
			}
		}
		else if(page_get_type(p) == VM_ANON | VM_MARKER_0){
			if(!vm_alloc_page_with_initializer(VM_ANON | VM_MARKER_0, p->va, p->writable,
				p->uninit.init, p->uninit.aux))
				{
					return false;
				}
			if(!vm_claim_page(p->va)){
				return false;
			}
		}
		else{
			return false;
		}
	}
	return true;
}

/* SPT의 리소스 보류 해제 */
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
	uint64_t hash = hash_bytes(&page->va, sizeof(page->va));

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

