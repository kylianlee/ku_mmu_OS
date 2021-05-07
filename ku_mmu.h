//
// Created by Kylian Lee on 2021/05/07.
//

#ifndef KU_MMU_OS__KU_MMU_H_
#define KU_MMU_OS__KU_MMU_H_
enum MASK{
  PD_MASK = 192,
  PMD_MASK = 48,
  PT_MASK = 12
};

enum SHIFT{
  PD_SHIFT = 6,
  PMD_SHIFT = 4,
  PT_SHIFT = 2,
  PFN_SHIFT = 2
};

enum MAPPING_STATE{
  UNMAPPED = 0,
  MAPPED,
  SWAPPED
};

typedef struct pcb{
  int PDBR;		//	page directory base register 의 index 번호
} PCB;

typedef struct node{
  void *PTE;
  struct node *next;
} Node;

typedef struct queue{
  int node_num;
  Node *front;
  Node *rear;
}Queue;

static PCB **ku_mmu_pcb;
static char *ku_mmu_manager;		//	pmemAddr 이 할당이 됐는지, 누구에게 할당이 됐는지 관리
static char *ku_mmu_swap_manager;	//	swapAddr 이 할당이 됐는지 관리
static int ku_mmu_page_num;
static int ku_mmu_swap_num;
static void *ku_mmu_pmemAddr;
static void *ku_mmu_swapAddr;
static Queue ku_mmu_queue;

void enqueue(void* pte);
void* dequeue();
int swap_out(void);
int swap_in(void *entry);
int find_swap_page();
int find_free_page();
int get_PDidx(int va);
int get_PMDidx(int va);
int get_PTidx(int va);
int is_mapped(void *entry);
int mapping_pages(void* entry);
int mapping_entries(void* entry);
char get_pfn(void *entry);

void enqueue(void* pte){
  Node *temp;
  temp = (Node *)malloc(sizeof(Node));
  temp->PTE = pte;
  temp->next = NULL;
  if(ku_mmu_queue.node_num == 0){
    ku_mmu_queue.front = temp;
    ku_mmu_queue.rear = temp;
  }
  else{
    ku_mmu_queue.rear->next = temp;
    ku_mmu_queue.rear = temp;
  }
  ku_mmu_queue.node_num++;

}

void* dequeue(){
  if(ku_mmu_queue.node_num == 0)
    return 0;
  void* ret = ku_mmu_queue.front->PTE;
  Node *temp = ku_mmu_queue.front;
  ku_mmu_queue.front = ku_mmu_queue.front->next;
  ku_mmu_queue.node_num--;
  free(temp);
  if(ku_mmu_queue.front==NULL){
    ku_mmu_queue.rear=NULL;
  }
  return ret;
}

int find_free_page(){
  int i;
  for(i = 1; i < ku_mmu_page_num; i++){
    if(ku_mmu_manager[i] == UNMAPPED){
      ku_mmu_manager[i] = MAPPED;
      return i;		//	사용 가능한 page frame number의 index 반환
    }
  }
  return 0;
}

int find_swap_page(){
  int i;
  for(i = 1; i < ku_mmu_swap_num; i++){
    if(ku_mmu_swap_manager[i] == UNMAPPED){
      ku_mmu_swap_manager[i] = MAPPED;
      return i;		//	사용 가능한 page frame number의 index 반환
    }
  }
  return 0;
}

int swap_in(void *entry){		//	entry = PTE의 주소
  int pfn = find_free_page();	//	pfn = free_page index
  if(pfn==0) pfn = swap_out();// free page fault
  if(pfn==-1)  return -1;		// swap space full
  int spn = *(char*)entry >> 1;
  ku_mmu_swap_manager[spn]=UNMAPPED;
  *(char*)entry = pfn << 2;
  *(char*)entry += 1;
  ku_mmu_manager[pfn]=MAPPED;
  enqueue(entry);
  return 0;
}

int swap_out(void){
  if(ku_mmu_queue.front==NULL) return -1;
  void *entry = ku_mmu_queue.front->PTE;
  int pfn = get_pfn(entry);
  int spn = find_swap_page();		//	spn = swap page number
  if(spn==0) return -1;
  *(char*)entry = spn << 1;		//	entry에 swap space offset 넣어줌
  *(char*)(ku_mmu_swapAddr + spn * 4) = pfn;		//	page 의 pfn(index)
  ku_mmu_manager[pfn] = UNMAPPED;
  ku_mmu_swap_manager[spn] = MAPPED;
  dequeue(entry);
  return pfn;
}

int get_PDidx(int va){
  int temp = va & PD_MASK;
  int PDidx = temp >> PD_SHIFT;
  return PDidx;
}

int get_PMDidx(int va){
  int temp = va & PMD_MASK;
  int PMDidx = temp >> PMD_SHIFT;
  return PMDidx;
}

int get_PTidx(int va){
  int temp = va & PT_MASK;
  int PTidx = temp >> PT_SHIFT;
  return PTidx;
}

int is_mapped(void *entry){
  char temp = *(char *)entry;
  if((temp & 255) == 0)
    return UNMAPPED;
  else if((temp & 1) == 0)
    return SWAPPED;
  else
    return MAPPED;
}

int mapping_pages(void* entry){		//	entry = physical address, call by reference
  char free_page_num = find_free_page();
  if(free_page_num == 0)
    return UNMAPPED;
  *(char *)entry = free_page_num << PFN_SHIFT;
  *(char *)entry += 1;
  enqueue(entry);
  return MAPPED;
}

int mapping_entries(void* entry){
  char free_page_num = find_free_page();
  if(free_page_num == 0)
    return UNMAPPED;
  *(char *)entry =  free_page_num << PFN_SHIFT;
  *(char *)entry += 1;
  return MAPPED;
}

char get_pfn(void *entry){
  char temp = *(char *)entry / 4;
  return temp;
}

void *ku_mmu_init(unsigned int pmem_size, unsigned int swap_size) {
  ku_mmu_pmemAddr = calloc(pmem_size, 1);
  ku_mmu_swapAddr = calloc(swap_size, 1);
  if(!(pmem_size % 4))
    ku_mmu_page_num = pmem_size / 4;
  else
    ku_mmu_page_num = pmem_size / 4 - 1;
  if(!(swap_size % 4))
    ku_mmu_swap_num = swap_size / 4;
  else
    ku_mmu_swap_num = swap_size / 4 - 1;
  ku_mmu_queue.node_num = 0;
  ku_mmu_queue.front = NULL;
  ku_mmu_queue.rear = NULL;
  ku_mmu_manager = (char *)calloc(ku_mmu_page_num, sizeof(char));
  ku_mmu_swap_manager = (char *)calloc(ku_mmu_swap_num, sizeof(char));
  ku_mmu_pcb = calloc(256, sizeof(PCB));		//	pcb 최대 개수만큼 할당, 인덱스가 pid
  return ku_mmu_pmemAddr;
}

int ku_run_proc(char pid, struct ku_pte **ku_cr3) {
  int free_page_idx;
  if(ku_mmu_pcb[pid] == 0){
    free_page_idx = find_free_page();		//	for PCB
    PCB *pcb = ku_mmu_pmemAddr + (free_page_idx * 4);
    free_page_idx = find_free_page();		//	for PDBR
    pcb->PDBR = free_page_idx; // change idx to pfn
    ku_mmu_pcb[pid] = pcb;
  }
  *ku_cr3 = ku_mmu_pmemAddr + ku_mmu_pcb[pid]->PDBR * 4;
  return 0;
}

int ku_page_fault(char pid, char va) {
  char PD_idx = get_PDidx(va);
  char PMD_idx = get_PMDidx(va);
  char PT_idx = get_PTidx(va);
  char mapping_state;
  int suc_or_fail, tmp;
  char *PDE = (char *)(ku_mmu_pmemAddr + (ku_mmu_pcb[pid]->PDBR * 4) + PD_idx);
  mapping_state = is_mapped(PDE);
  if(mapping_state == UNMAPPED){
    suc_or_fail = mapping_entries(PDE);
    if(suc_or_fail == UNMAPPED){
      tmp = swap_out();
      if(tmp == -1) return -1;
      suc_or_fail = mapping_entries(PDE);
      if(suc_or_fail == UNMAPPED)
        printf("Not Enough Swap Space\n");
    }
  }
  char *PMDE = (char *)(ku_mmu_pmemAddr + 4 * get_pfn(PDE) + PMD_idx);
  mapping_state = is_mapped(PMDE);
  if(mapping_state == UNMAPPED){
    suc_or_fail = mapping_entries(PMDE);
    if(suc_or_fail == UNMAPPED){
      tmp = swap_out();
      if(tmp == -1) return -1;
      suc_or_fail = mapping_entries(PMDE);
      if(suc_or_fail == UNMAPPED)
        printf("Not Enough Swap Space\n");
    }
  }
  char *PTE = (char *)(ku_mmu_pmemAddr + 4 * get_pfn(PMDE) + PT_idx);
  mapping_state = is_mapped(PTE);
  if(mapping_state == UNMAPPED){
    suc_or_fail = mapping_pages(PTE);
    if(suc_or_fail == UNMAPPED){
      tmp = swap_out();
      if(tmp == -1) return -1;
      suc_or_fail = mapping_pages(PTE);
      if(suc_or_fail == UNMAPPED)
        printf("Not Enough Swap Space\n");
    }
  }
  else if(mapping_state == SWAPPED){
    if(swap_in(PTE)==-1) return -1;
  }

  return 0;
}


#endif //KU_MMU_OS__KU_MMU_H_
