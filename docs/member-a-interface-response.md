
# KERNEL THREAD PROJECT — MEMBER A/B INTERFACE RESPONSE

* **Người gửi:** Đỗ Trọng Nam-20233541 — Thành viên A (Shared address space, page-table lifetime & memory reference counting)


* **Người nhận:** Member B (Kernel-thread lifecycle & syscall semantics)


* **Mục tiêu:** Thống nhất mô hình sở hữu không gian địa chỉ (address-space ownership), Memory API, quy tắc Reference Counting, thứ tự khóa (lock ordering) trên đa nhân (`CPUS=4`) và điểm tích hợp với vòng đời thread trước khi bước vào giai đoạn code.



---

## 1. Current code facts

Trạng thái mã nguồn hiện tại trên nhánh làm việc `kernel-thread-lifecycle`:

* **Cấu trúc quản lý tiến trình (`kernel/proc.h`, `kernel/proc.c`):** Kernel hiện tại thuần túy là process-only; mỗi `struct proc` giữ độc quyền `p->pagetable`, `p->sz`, `p->trapframe`, `p->kstack`, `p->ofile` và `p->cwd`. Chưa có cấu trúc đại diện cho nhóm luồng hay bộ đếm tham chiếu không gian địa chỉ (`addrspace`).


* **Sao chép và giải phóng bộ nhớ (`kfork`, `freeproc`):** `kfork()` luôn tạo một bảng trang mới và nhân bản toàn bộ RAM vật lý bằng `uvmcopy()`. Khi một `proc` bị thu hồi trong `freeproc()`, nó lập tức giải phóng `p->trapframe` và gọi `proc_freepagetable(p->pagetable, p->sz)` để hủy toàn bộ ánh xạ cùng các trang vật lý user. Nếu hai thread dùng chung `p->pagetable`, thread đầu tiên gọi `freeproc()` sẽ gây *use-after-free* ngay lập tức cho các thread còn lại.


* **Xung đột địa chỉ ảo `TRAPFRAME` (`kernel/memlayout.h`, `kernel/trap.c`):** Mọi `proc` hiện tại đều ánh xạ trang vật lý `p->trapframe` vào cùng một địa chỉ ảo cố định là `TRAPFRAME` (`TRAMPOLINE - PGSIZE`). Khi nhiều thread chia sẻ chung một `pagetable_t`, chúng không thể ánh xạ các `p->trapframe` khác nhau vào cùng một địa chỉ ảo `TRAPFRAME` duy nhất.
* **Thay đổi kích thước và thay thế vùng nhớ (`growproc`, `exec`):** `growproc()` đang thao tác trực tiếp trên `p->pagetable` và `p->sz` mà không có khóa bảo vệ chung giữa các luồng. `exec()` thay thế `p->pagetable` và `p->sz` của riêng `myproc()` rồi gọi `proc_freepagetable()` xóa ngay bảng trang cũ.



---

## 2. Proposed memory API

Để tách biệt rõ ràng phần quản lý vòng đời luồng (Member B) và quản lý bảng trang/bộ nhớ (Member A), Member A đề xuất đóng gói không gian địa chỉ dùng chung vào cấu trúc độc lập `struct addrspace`:

### Phân tích Tradeoff giữa 2 phương án thiết kế:

* **Phương án khuyến nghị (Independent `struct addrspace` object + Per-thread Trapframe Slot):**
* **Cơ chế:** Tạo đối tượng `struct addrspace` chứa `pagetable`, `sz`, `refcount`, `lock` và bitmap quản lý các slot `TRAPFRAME` (`TRAPFRAME - slot * PGSIZE`). Mọi thread trong cùng process trỏ `p->as` về cùng một `struct addrspace`.
* **Ưu điểm:** Không phụ thuộc vào việc "leader thread" còn sống hay đã `thread_exit()`; `p->as->sz` luôn nhất quán cho mọi thread khi gọi `sbrk()`; an toàn tuyệt đối trên `CPUS=4` vì mỗi thread có một địa chỉ ảo `p->trapframe_va` riêng trong cùng một `pagetable`.
* **Tradeoff:** Cần quản lý một bảng tĩnh `addrspace_table[NPROC]` trong kernel và sửa 1 dòng truyền `p->trapframe_va` trong `usertrapret()`.


* **Phương án dự phòng (Đặt `refcount` vào Leader Proc hoặc gắn mảng theo `pagetable`):**
* **Ưu điểm:** Không cần thêm struct mới.
* **Nhược điểm:** Rất phức tạp nếu Leader Thread gọi `thread_exit()` trước các Worker Threads; đồng thời cực kỳ dễ lệch biến `p->sz` giữa các `struct proc` khi một thread gọi `growproc()`.
👉 **Quan điểm của A:** Khuyến nghị sử dụng **Phương án 1 (`struct addrspace` độc lập)**.





### Cấu trúc dữ liệu và Nguyên mẫu hàm (Prototypes trong `kernel/defs.h`):

```c
#define MAX_THREADS_PER_AS 16
#define THREAD_TRAPFRAME(slot) (TRAPFRAME - (slot) * PGSIZE)

struct addrspace {
  struct spinlock lock;      // Bảo vệ sz, pagetable mutations, trapframe_bitmap
  struct spinlock ref_lock;  // Spinlock nhẹ bảo vệ riêng refcount (có thể gộp vào lock)
  pagetable_t pagetable;     // Bảng trang dùng chung
  uint64 sz;                 // Kích thước vùng nhớ user hiện tại
  int refcount;              // Số lượng struct proc đang giữ tham chiếu
  uint16 tf_used_bitmap;     // Đánh dấu slot TRAPFRAME (0..15) đang được sử dụng
  int valid;                 // Trạng thái đang cấp phát của struct addrspace
};

```

### Đặc tả hợp đồng chi tiết cho từng API

:

#### 1) `struct addrspace* as_alloc(void)`

* **Mục đích:** Cấp phát một `struct addrspace` mới, tạo `pagetable` gốc (đã map sẵn `TRAMPOLINE`), khởi tạo `refcount = 1`, `sz = 0`, và cấp slot trapframe `0` cho tiến trình đầu tiên.
* **Ai gọi & Lúc nào:** Member B gọi trong `allocproc()` (khi tạo process gốc như `userinit`, `kfork`) hoặc trong `exec()` khi chuẩn bị không gian địa chỉ mới.


* **Kết quả:** Trả về con trỏ `struct addrspace*` hợp lệ nếu thành công; trả về `0 (NULL)` nếu hết slot hoặc hết RAM (`kalloc` thất bại).


* **Có thể sleep không:** Không.


* **Lock caller được giữ:** Có thể giữ `p->lock`.


* **Chuyển giao Ownership & Rollback:** Nếu thành công, caller (`proc`) nhận ownership của 1 reference (`refcount = 1`). Nếu thất bại, hàm tự dọn dẹp nội bộ, không để lại state cần rollback.



#### 2) `int as_retain_for_clone(struct addrspace *as, struct proc *np)`

* **Mục đích:** Retain/acquire tham chiếu không gian địa chỉ cho thread mới (`refcount++`), cấp phát 1 `thread_slot` trống và ánh xạ trang vật lý `np->trapframe` vào `THREAD_TRAPFRAME(slot)` trên bảng trang chung, đồng thời gán `np->as = as`, `np->pagetable = as->pagetable`, `np->sz = as->sz`, `np->trapframe_va = THREAD_TRAPFRAME(slot)`.


* **Ai gọi & Lúc nào:** Member B gọi trong `clone()` sau khi đã `allocproc()` cho `np` và đã cấp phát trang vật lý `np->trapframe`.


* **Kết quả:** Trả về `0` nếu thành công; trả về `-1` nếu vượt quá `MAX_THREADS_PER_AS` hoặc lỗi cấp phát PTE trung gian khi map trapframe.


* **Có thể sleep không:** Không.


* **Lock caller được giữ:** Caller có thể giữ `np->lock` (không giữ `as->lock` trước khi gọi).


* **Chuyển giao Ownership & Rollback:** Khi trả về `0`, `np` chính thức giữ 1 reference vào `as`. Nếu trả về `-1`, `refcount` không tăng, không có PTE rác, Member B chỉ cần dọn dẹp `np` (gọi `kfree(np->trapframe)`). If `clone()` gặp lỗi ở các bước sau đó của Member B, Member B bắt buộc gọi `as_release(np)` để rollback.



#### 3) `void as_release(struct proc *p)`

* **Mục đích:** Gỡ ánh xạ `p->trapframe` khỏi slot `THREAD_TRAPFRAME(p->tf_slot)` (không `kfree` trang vật lý `p->trapframe` vì `freeproc` của B tự `kfree`), trả lại slot vào bitmap, và giảm `as->refcount--`. Nếu `refcount` về `0`, hàm tự động gọi `proc_freepagetable(as->pagetable, as->sz)` để giải phóng toàn bộ user pages và bảng trang, rồi thu hồi `struct addrspace`.


* **Ai gọi & Lúc nào:** Member B gọi trong `freeproc(struct proc *p)` khi thu hồi (reap) thread/process ở `join()` hoặc `wait()`, hoặc trên đường rollback khi `clone()` thất bại.


* **Kết quả:** `void` (luôn thành công).


* **Có thể sleep không:** Không (`kfree` và `uvmfree` chỉ dùng spinlock).


* **Lock caller được giữ:** Caller giữ `p->lock` (và có thể giữ `wait_lock`), tuyệt đối không giữ `as->lock` trước khi gọi.


* **Chuyển giao Ownership:** `p` từ bỏ hoàn toàn quyền truy cập vào `as` (`p->as = 0`, `p->pagetable = 0`, `p->sz = 0`).



#### 4) `int as_grow(struct addrspace *as, int n)`

* **Mục đích:** Resize (mở rộng hoặc thu hẹp) vùng nhớ heap dùng chung một cách an toàn giữa nhiều luồng (`sbrk`/`growproc`), cập nhật `as->sz` và đồng bộ lại `myproc()->sz`. Chặn không cho `as->sz` mở rộng chạm vào vùng `THREAD_TRAPFRAME(MAX_THREADS_PER_AS - 1)`.


* **Ai gọi & Lúc nào:** Member B gọi trong `growproc(int n)` (syscall `sbrk`).


* **Kết quả:** Trả về `0` nếu thành công, `-1` nếu thất bại (hết RAM hoặc vượt giới hạn địa chỉ).


* **Có thể sleep không:** Không.


* **Lock caller được giữ:** Caller **không** giữ `p->lock` khi gọi `as_grow()` để giữ đúng thứ tự khóa.


* **Rollback khi lỗi:** Nếu `uvmalloc` thất bại giữa chừng, `uvmalloc` tự động gọi `uvmdealloc` quay về kích thước `old_sz` ban đầu, không để lại trang cấp phát dở dang.



#### 5) `int as_copy_for_fork(struct addrspace *src_as, struct proc *child_p)`

* **Mục đích:** Tạo một `struct addrspace` mới độc lập (`refcount = 1`) cho tiến trình con khi gọi `fork()`, đồng thời giữ `src_as->lock` trong suốt quá trình `uvmcopy()` để tránh xung đột với thread khác đang gọi `sbrk()`.


* **Ai gọi & Lúc nào:** Member B gọi trong `kfork()`.


* **Kết quả:** `0` nếu thành công, `-1` nếu hết bộ nhớ.



#### 6) `void as_replace_on_exec(struct proc *p, struct addrspace *new_as)`

* **Mục đích:** Thay thế không gian địa chỉ của `p` sang `new_as` (`refcount = 1`) tại điểm commit cuối cùng của `exec()`, và gọi `as_release` trên `old_as` của `p`.


* **Ai gọi & Lúc nào:** Gọi trong `exec()` sau khi đã nạp xong ELF vào `new_as`.



#### 7) `int as_copyin(struct addrspace *as, char *dst, uint64 srcva, uint64 len)` & `as_copyout(...)`

* **Mục đích:** Bọc `copyin`/`copyout`/`copyinstr` với cơ chế khóa `as->lock` để ngăn ngừa race condition khi một thread đang đọc/ghi buffer trong syscall thì một thread anh em khác gọi `sbrk(-n)` thu hẹp bộ nhớ làm giải phóng trang vật lý đó.



---

## 3. Ownership/refcount rules

Trả lời trực tiếp các câu hỏi ở Mục 1 của Member B:

* **Owner logic của shared address space:** Chính là đối tượng `struct addrspace` độc lập. Không một thread đơn lẻ nào (kể cả main/leader thread) sở hữu độc quyền `pagetable`. Mọi thread còn sống hoặc chưa được reap (thu hồi) đều là đồng sở hữu thông qua một reference (`p->as`).


* **Những tham chiếu nào giữ cho address space còn sống:** Mỗi `struct proc` đang ở trạng thái từ sau `allocproc`/`clone` cho tới trước khi `freeproc()` hoàn tất sẽ giữ đúng **1** đơn vị `refcount`.


* **Tại sao chỉ giảm reference ở `freeproc()` (khi reap/join/wait) mà KHÔNG giảm ngay trong `exit()`/`thread_exit()`?**


* Trong xv6, khi một luồng gọi `exit()` hoặc `thread_exit()`, nó vẫn đang thực thi trên CPU cho đến khi gọi `sched()` để chuyển ngữ cảnh sang scheduler. Ngoài ra, việc trì hoãn `as_release(p)` tới đúng hàm `freeproc()` (khi thread đã ở trạng thái `ZOMBIE`, chắc chắn không còn chạy trên bất kỳ CPU nào và đang được thu hồi bởi `join()` hoặc `wait()`) giúp loại bỏ hoàn toàn nguy cơ giải phóng `pagetable` khi thread đó hoặc thread khác vẫn đang ở trong trap handler hoặc quá trình context switch.




* **Điều kiện chính xác để lần release cuối giải phóng page table và user pages:** Khi `as_release(p)` giảm `as->refcount` từ `1` xuống `0`. Tại thời điểm đó, mọi thread từng tham chiếu tới `as` đều đã kết thúc thực thi và đã bước vào `freeproc()`.



### Bảng quy tắc Acquire / Release Reference Count

:

| Sự kiện / Hàm | Chủ thể thực hiện | Thay đổi `refcount` | Thời điểm & Điều kiện chính xác |
| --- | --- | --- | --- |
| Tạo process đầu (`userinit` / `kfork`) | `allocproc` / `kfork` | `0 -> 1` | Khi cấp phát `struct addrspace` mới cho process gốc.

 |
| Tạo thread mới (`clone`) | `as_retain_for_clone` | `k -> k + 1` | Trong `clone()`, sau khi cấp phát `np` và map thành công slot `TRAPFRAME` của `np`.

 |
| Lỗi giữa chừng khi `clone` | `clone` (rollback) | `k + 1 -> k` | Nếu bước thiết lập sau `as_retain_for_clone` bị lỗi, gọi `as_release(np)` trong đường dọn dẹp lỗi.

 |
| Thread gọi `thread_exit` / `exit` | Thread đang thoát | Giữ nguyên (`k`) | Chỉ đổi state sang `ZOMBIE`, **chưa** giảm `refcount` vì thread vẫn cần trang nhớ cho tới khi rời CPU hoàn toàn.

 |
| Thu hồi thread (`join` / `wait`) | `freeproc(p)` | `k -> k - 1` | Khi `p` đã là `ZOMBIE` và được thu hồi. Nếu `k - 1 > 0`: chỉ unmap trapframe của `p`. Nếu `k - 1 == 0`: giải phóng toàn bộ `pagetable` và user pages.

 |
| Gọi `exec()` thành công | `as_replace_on_exec` | `old_as: k -> k - 1`<br>

<br>`new_as: 0 -> 1` | Thread gọi `exec` chuyển sang `new_as` và nhả 1 ref của `old_as`.

 |

---

## 4. Locking and concurrency

Trả lời chi tiết Mục 3 của Member B về đồng bộ hóa và an toàn trên nhiều CPU (`CPUS=4`):

* **Loại khóa trong `struct addrspace`:** Sử dụng `struct spinlock lock` trong `struct addrspace`. Vì các thao tác `kalloc()`, `kfree()`, `mappages()`, `uvmunmap()`, `copyin()`, `copyout()` trong xv6 đều không bao giờ gọi `sleep()` (chúng chỉ thao tác RAM), dùng `spinlock` là nhanh và tương thích hoàn toàn với quy tắc khóa của xv6.


* **Thứ tự khóa toàn cục (Global Lock Hierarchy):** Để ngăn chặn tuyệt đối Deadlock với hệ thống khóa sẵn có của xv6, thứ tự lấy khóa bắt buộc là:



$$\text{wait\_lock} \longrightarrow \text{p->lock} \longrightarrow \text{as->lock} \longrightarrow \text{kmem.lock}$$


* *Hệ quả quan trọng:* Khi đang giữ `as->lock`, code của Member A **không bao giờ** được gọi ngược lại `acquire(&p->lock)` hoặc `acquire(&wait_lock)`.




* **Quy tắc Sleep (Sleep Rules):** Không được gọi `sleep()` khi đang giữ `as->lock` (do `as->lock` là spinlock tắt ngắt CPU). Nếu `exec()` cần đọc đĩa (có thể sleep trong `loadseg`), `exec()` phải nạp dữ liệu vào `new_pagetable` (chưa chia sẻ cho ai nên không cần giữ `old_as->lock`), hoặc nếu cần đồng bộ thì tách riêng khỏi vùng giữ `spinlock`.


* **Làm sao ngăn `pagetable` bị free khi thread khác đang ở user mode, trap handler hoặc syscall?**


* Một thread đang ở user mode, đang xử lý trong trap handler (`usertrap`), hoặc đang nằm giữa một syscall (kể cả đang `sleep` chờ I/O) đều có trạng thái `RUNNING`, `RUNNABLE` hoặc `SLEEPING` — tức là **chưa thể chuyển sang `ZOMBIE` và chưa thể bị gọi `freeproc()**`. Do đó, chính thread đó vẫn đang giữ ít nhất **1** đơn vị `as->refcount >= 1`. Không một thread nào khác có thể làm `refcount` giảm về `0` để giải phóng `pagetable` dưới chân thread đang chạy.




* **Đồng bộ hóa giữa `growproc()`, fault và `copyin`/`copyout`:**


* Mọi thao tác sửa đổi cấu trúc bảng trang hoặc thay đổi `as->sz` (`as_grow` khi `sbrk`) và mọi thao tác đi bộ trên bảng trang (`as_copyin`, `as_copyout`, page fault/lazy allocation nếu có) đều phải giữ `acquire(&as->lock)`.


* Nhờ `as->lock`, nếu Thread 1 đang chạy `copyin()` đọc dữ liệu từ vùng đỉnh heap thì Thread 2 gọi `sbrk(-4096)` sẽ phải chờ `copyin()` hoàn tất rồi mới được unmap và `kfree()` trang vật lý đó, chặn đứng lỗi *use-after-free* trong kernel.


* Khi `as_grow()` hoàn tất, các thread đọc kích thước bộ nhớ luôn đọc qua `p->as->sz` thay vì biến cục bộ `p->sz` bị cũ (stale).





---

## 5. Member B integration points

Trả lời Mục 4 và Mục 5 của Member B về tương tác lifecycle và các điểm cần móc nối trong code:

| File / Hàm của Member B | Thay đổi tích hợp cần gọi sang API của Member A | Ràng buộc Memory Subsystem cần tuân thủ |
| --- | --- | --- |
| `kernel/proc.h` (`struct proc`) | Thêm trường `struct addrspace *as;`, `int tf_slot;`, và `uint64 trapframe_va;` | Dùng `p->as->sz` làm nguồn kích thước chuẩn (hoặc cập nhật `p->sz = p->as->sz` qua helper).

 |
| `kernel/proc.c` (`allocproc` / `userinit`) | Gọi `p->as = as_alloc()` khi khởi tạo tiến trình gốc đầu tiên | Thiết lập `p->trapframe_va = THREAD_TRAPFRAME(0)`.

 |
| `kernel/proc.c` (`clone`) | Gọi `as_retain_for_clone(p->as, np)` sau khi cấp phát `np` | Nếu `clone` thất bại sau bước này, phải gọi `as_release(np)` trước khi trả lỗi.

 |
| `kernel/proc.c` (`freeproc`) | Thay lời gọi `proc_freepagetable(p->pagetable, p->sz)` bằng `as_release(p)` | Vẫn giữ `kfree(p->trapframe)` của riêng `p`, nhưng việc giải phóng `pagetable` do `as_release(p)` quyết định dựa trên `refcount`.

 |
| `kernel/proc.c` (`growproc`) | Thay đoạn gọi `uvmalloc`/`uvmdealloc` trực tiếp bằng `as_grow(p->as, n)` | Không giữ `p->lock` trước khi gọi `as_grow()`.

 |
| `kernel/trap.c` (`usertrapret`) | Thay hằng số `TRAPFRAME` khi chuẩn bị nhảy về `userret` bằng `p->trapframe_va` | Đảm bảo mỗi thread trên `CPUS=4` lưu/khôi phục thanh ghi vào đúng trang `p->trapframe` của mình.

 |
| `kernel/proc.c` (`kfork`) | Gọi `as_copy_for_fork(p->as, np)` thay cho `uvmcopy` trần | Đảm bảo chỉ copy không gian địa chỉ sang 1 tiến trình con độc lập có `refcount = 1` trong khi khóa `p->as->lock`.

 |
| `kernel/exec.c` (`exec`) | Tạo `new_as = as_alloc()`, nạp ELF, rồi gọi `as_replace_on_exec(p, new_as)` | Xem mục 6 bên dưới về yêu cầu dừng các thread anh em khi `exec()`.

 |

* **Các file Member A dự kiến thay đổi:** `kernel/vm.c`, `kernel/kalloc.c`, `kernel/defs.h` (và bổ sung phần quản lý `struct addrspace` trong `kernel/vm.c` hoặc file chung theo thống nhất).


* **API có thể cung cấp trước (Stub/Phase 1):** Member A có thể cung cấp ngay bộ hàm `as_alloc`, `as_retain_for_clone`, `as_release`, `as_grow` trong vòng 24–48h đầu tiên sau khi chốt thiết kế để Member B lắp ngay vào nhánh `kernel-thread-lifecycle`.



---

## 6. Open decisions for the whole team

Theo đúng nguyên tắc không tự quyết định syscall semantics thay nhóm, Member A nêu rõ các điểm cần cả 3 thành viên (A, B, C) chốt duyệt:

1. **Hành vi của `exec()` khi nhóm đang có nhiều thread:**


* *Góc độ Memory Subsystem (Member A):* Về mặt bộ nhớ, `as_replace_on_exec()` hoàn toàn có thể tách riêng thread gọi `exec()` sang `new_as` (`refcount = 1`) và chỉ giảm `old_as->refcount` đi 1 (không làm crash `old_as` nếu các thread cũ chưa thoát). Tuy nhiên, theo chuẩn POSIX và đề xuất từ Member C, `exec()` trong tiến trình đa luồng cần đánh dấu hủy (kill/terminate) toàn bộ các thread anh em trong nhóm trước khi chạy chương trình mới. **Cần B và C xác nhận:** B sẽ force-kill các sibling threads khi `exec()` hay từ chối (`return -1`) nếu `as->refcount > 1`?




2. **Ngữ nghĩa của `exit()`, `thread_exit()`, `kill(pid)` và `wait()` vs `join()`:**


* *Yêu cầu từ Memory Subsystem:* Bất kể một thread chết do `thread_exit()`, do process `exit()`, hay do `kill()`, bộ nhớ `addrspace` chỉ yêu cầu một bất biến duy nhất: **Mỗi thread được tạo ra phải đi qua `freeproc()` đúng 1 lần sau khi đã ngừng chạy hoàn toàn trên CPU** để trả lại `refcount` về `0`.


* *Cần cả nhóm chốt:* Nếu main thread gọi `exit()`, Member B sẽ đánh dấu `killed = 1` cho tất cả worker threads và thu hồi (reap) chúng ở đâu để `refcount` giảm hết về `0` mà cha của process chỉ cần gọi `wait()` 1 lần?




3. **Số lượng thread tối đa trên một Address Space (`MAX_THREADS_PER_AS`):**


* Để xếp các slot `THREAD_TRAPFRAME(slot)` nằm gọn phía trên vùng Heap mà không tốn quá nhiều địa chỉ ảo, Member A đề xuất giới hạn `MAX_THREADS_PER_AS = 16` (tương ứng 16 trang `TRAPFRAME` = 64KB ngay dưới `TRAMPOLINE`). Cần B và C xác nhận con số 16 này có đủ cho toàn bộ test suite (bao gồm bài test 8 threads parallel sum và 1.000 chu kỳ tuần tự) hay không.




4. **Syscall kiểm tra số trang trống (`countfree`) cho bài test Leak:**


* Member A sẽ viết hàm `kalloc_countfree()` trong `kernel/kalloc.c`. Cần B và C chốt cách xuất giá trị này lên user space (thêm 1 syscall phụ hay kiểm tra trong kernel test) để phục vụ bài test 1.000 chu kỳ `clone`/`join`.





---

## 7. Tests and failure cases

Trả lời Mục 5 của Member B về kiểm thử và các kịch bản lỗi/race condition cần chặn:

### Các Failure / Race Cases bắt buộc phải chặn trước khi coi API là ổn định

:

1. **Premature Page Table Free (Giải phóng sớm):** Thread A tạo Thread B; Thread A kết thúc và được reap trước khi Thread B chạy xong. Bảng trang tuyệt đối không được giải phóng khi `refcount` giảm từ `2` xuống `1`.


2. **Double Release / Memory Leak sau 1.000 chu kỳ:** Chạy vòng lặp 1.000 lần `clone()` + `join()`. Nếu thiếu `as_release` hoặc quên unmap `THREAD_TRAPFRAME(slot)`, hệ thống sẽ cạn sạch RAM (`kalloc` trả về 0). Nếu giảm `refcount` 2 lần cho 1 thread, hệ thống sẽ panic `freewalk` hoặc `kfree`.


3. **Partial `clone()` Failure Rollback:** Khi `clone()` thất bại ở bước cấp phát hoặc khi hết slot `proc` / hết RAM giữa chừng, `refcount` và slot `TRAPFRAME` phải được hoàn trả chính xác về trạng thái trước khi gọi `clone()`.


4. **Concurrent `sbrk()` (`growproc`) trên `CPUS=4`:** Nhiều thread cùng gọi `malloc()`/`sbrk()` tăng/giảm heap đồng thời trên 4 nhân CPU; không được xảy ra panic `remap` và kích thước `as->sz` phải đồng bộ cho mọi thread.


5. **Concurrent `sbrk(-n)` vs `copyin`/`copyout`:** Một thread truyền con trỏ vùng đỉnh heap vào syscall (như `write`), trong khi thread khác gọi `sbrk` thu hẹp heap; `as->lock` phải tuần tự hóa để kernel không đọc vào trang vật lý đã bị `kfree()`.


6. **Multi-CPU `TRAPFRAME` Collision:** Hai thread cùng `addrspace` chạy đồng thời trên 2 CPU core và cùng bị timer interrupt; nhờ `p->trapframe_va` tách rời theo slot, thanh ghi của hai thread không bao giờ ghi đè lên nhau.



### Kế hoạch phối hợp kiểm thử với Member B và Member C

:

* **Test nội bộ Member A (chạy độc lập):** Kiểm thử đơn vị vòng đời `as_alloc` $\rightarrow$ `as_retain_for_clone` (N lần) $\rightarrow$ `as_grow` $\rightarrow$ `as_release` (N+1 lần) và đối chiếu `kalloc_countfree()` trước/sau bằng nhau tuyệt đối.


* **Test tích hợp với Member B & C:**
* `test_shared_heap_sbrk`: Thread 1 gọi `sbrk()` cấp phát vùng nhớ mới và ghi dữ liệu, Thread 2 đọc ngay địa chỉ đó mà không bị page fault.


* `test_leader_exit_first` / `test_worker_exit_first`: Kiểm tra thứ tự kết thúc ngược nhau giữa các luồng không gây lỗi truy cập bộ nhớ.


* `test_1000_cycles_leak`: Phối hợp cùng Member B (`clone`/`join`) và Member C (user test) xác nhận số trang vật lý trống sau 1.000 chu kỳ không đổi.





---

## 8. Approval status

* **Trạng thái:** **ĐỀ XUẤT (PROPOSED — Chờ Member B và Member C review & phản hồi)**.


* Không sửa code và không tạo commit cho đến khi Member B xác nhận đồng ý với giao diện `struct addrspace`, quy ước `p->trapframe_va` và chốt các mục ở phần *Open decisions*.