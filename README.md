<p align="center"><img src="gfx/ntpsyncui.svg" width="88" height="88" alt="NTP Time Sync"></p>

<h1 align="center">NTP Time Sync</h1>

<p align="center">
Giữ đồng hồ Nokia E72 luôn đúng giờ bằng NTP qua Wi-Fi (Symbian S60 3rd Edition FP2).<br>
<a href="https://minhkhoi-dev.github.io/ntp-time-sync-s60/">Trang giới thiệu</a> ·
<a href="https://github.com/minhkhoi-dev/ntp-time-sync-s60/releases/latest">Tải bản mới nhất</a> ·
<a href="README.en.md">English</a>
</p>

<p align="center"><a href="https://github.com/minhkhoi-dev/ntp-time-sync-s60/actions/workflows/build.yml"><img src="https://github.com/minhkhoi-dev/ntp-time-sync-s60/actions/workflows/build.yml/badge.svg" alt="Build"></a></p>

Gói cài NTP Time Sync có hai chương trình:

- `ntpsyncd.exe`: dịch vụ nhỏ chạy nền, không có màn hình. Nó tự chạy khi bật
  máy và giữ đồng hồ đúng giờ bằng NTP qua Wi-Fi.
- **NTP Time Sync** (`ntpsyncui.exe`): ứng dụng trong menu, cho xem dịch vụ đang
  làm gì và đổi cài đặt. Đóng app không làm dừng việc đồng bộ.

## Tính năng

- **Tự chạy khi bật máy** (Startup List Management API) và ngay sau khi cài.
  Đồng bộ khoảng một phút sau khi bật máy, rồi cứ 6 giờ một lần (chọn được từ
  15 phút tới 1 tuần).
- **Chỉ dùng Wi-Fi.** Trước hết dùng kết nối Wi-Fi đang có; các điểm truy cập
  Wi-Fi đã lưu trong cài đặt thì nó được tự bật, không hỏi. Nó không bao giờ bật
  gì khác ngoài Wi-Fi, nên không cần SIM và không tốn dữ liệu di động.
- **Chọn Wi-Fi một lần.** Ngay sau khi cài, hoặc khi chọn **Đồng bộ ngay** mà chưa
  lưu điểm truy cập nào, máy hiện danh sách điểm truy cập Wi-Fi và lưu cái bạn
  chọn. Nó không bao giờ hỏi lúc bật máy hay khi đồng bộ theo lịch.
- **Đo cẩn thận.** Mỗi lần đồng bộ lấy tối đa 3 mẫu, giữ mẫu có thời gian khứ hồi
  ngắn nhất. Gói trả lời được kiểm tra đúng như RFC 4330 yêu cầu (mode, version,
  stratum, kiss-o'-death, originate timestamp, độ trễ hợp lý).
- **Không chỉnh vặt.** Lệch dưới 0,5 giây (chỉnh được) thì để yên. Máy được đặt
  giờ UTC bằng `User::SetUTCTime`; múi giờ của máy lo phần còn lại.
- **Canh khi giờ bị đổi.** Nếu thứ khác đổi giờ máy (giờ nhà mạng, ứng dụng khác),
  30 giây sau nó đồng bộ lại, tối đa 15 phút một lần nên không bao giờ giằng co.
  Nó phân biệt những lần đổi đó với lần chỉnh của chính nó bằng cách so đồng hồ
  với bộ đếm nhịp của máy (thứ không bị ảnh hưởng khi đặt giờ). Cứ 5 phút nó cũng
  so một lần, nên cả những lần giờ nhảy mà hệ thống không báo cũng hiện trong
  nhật ký.
- **Nhật ký chỉ nằm trên máy**, ở `C:\Data\ntpsyncd.log` (hai file, mỗi file tối
  đa 64 KB).

## Ứng dụng

Mở **NTP Time Sync** trong menu (nằm ở chỗ các ứng dụng mới cài, thường là
Applications). Màn hình bằng tiếng Việt hoặc tiếng Anh (Cài đặt → Ngôn ngữ).

- **Màn hình trạng thái**: chạy nền đang bật hay tắt (hay đang đồng bộ), lần đồng
  bộ thành công gần nhất và nó đã chỉnh đồng hồ bao nhiêu, lần thử gần nhất nếu
  thất bại và lý do, server và Wi-Fi đã dùng, và lần đồng bộ tới. Màn hình tự cập
  nhật khi đang mở.
- **Đồng bộ ngay** (trong Tùy chọn, hoặc bấm phím giữa): đồng bộ liền.
- **Cài đặt**: các điểm truy cập Wi-Fi được tự bật (đánh dấu trong danh sách
  Wi-Fi của máy), chu kỳ đồng bộ, ngưỡng chỉnh giờ, theo dõi đổi giờ, server NTP
  (mỗi dòng một server, Enter để xuống dòng, dùng tối đa 5) và ngôn ngữ. Mỗi thay
  đổi được lưu vào `C:\Data\ntpsyncd.ini` và áp dụng ngay.
- **Xem log**: phần cuối của `C:\Data\ntpsyncd.log`, dòng mới nhất ở trên.
- **Tắt chạy nền / Bật chạy nền**: dừng dịch vụ và giữ nó tắt cả sau khi khởi
  động lại (`enabled=0`), hoặc chạy lại.

Trạng thái nằm trong bộ nhớ của dịch vụ, nên sau khi khởi động lại máy, app hiện
"Chưa có" cho tới lần đồng bộ đầu tiên, khoảng một phút sau khi bật máy.

## Cài đặt

Cần có:

- **Nokia E72** (S60 3rd Edition Feature Pack 2, Symbian OS 9.3). NTP Time Sync
  được viết và chạy thử trên máy này. Các máy S60 3rd Edition khác có thể chạy
  được nhưng chưa được thử.
- **Máy đã hack** (vá InstallServer). Dịch vụ cần quyền `WriteDeviceData` để đặt
  giờ và `ReadDeviceData` để đọc danh sách điểm truy cập (app cần
  `ReadDeviceData`); file tự ký chỉ có những quyền đó trên máy đã vá. Xem
  [Vì sao cần máy đã hack](#vì-sao-cần-máy-đã-hack).

Các bước:

1. Nếu máy có SIM, **tắt** Ctrl. panel → Settings → General → Date and time →
   **Network operator time**. Nếu không, nhà mạng và NTP Time Sync sẽ thay nhau
   chỉnh giờ.
2. Tải `ntp-time-sync-x.y.z.sisx` ở
   [Releases](https://github.com/minhkhoi-dev/ntp-time-sync-s60/releases/latest).
   Trên điện thoại, mở [bản nhẹ của trang](https://minhkhoi-dev.github.io/ntp-time-sync-s60/m/)
   bằng trình duyệt của máy hoặc Opera Mini.
3. Cài đặt. Mọi thứ vào bộ nhớ máy (C:), vì mục tự chạy khi bật máy trỏ tới
   `c:\sys\bin\ntpsyncd.exe`. Dịch vụ chạy ngay sau khi cài.
4. Vài giây sau, máy hiện danh sách điểm truy cập. Chọn Wi-Fi của bạn. Nó đồng
   bộ qua Wi-Fi đó và thêm một dòng vào `C:\Data\ntpsyncd.ini`:

   ```
   iap=13    ; HomeWifi (picked from the menu)
   ```

   Từ đó nó được tự bật điểm truy cập này mà không hỏi. Nếu chọn Easy WLAN
   ("Search for WLAN"), nó chỉ dùng cho lần đó mà không lưu, vì lần sau bật lên sẽ
   lại mở bảng chọn mạng. Nếu bạn bấm hủy, lần **Đồng bộ ngay** sau nó hỏi lại.
5. Mở **NTP Time Sync** để xem trạng thái, đồng bộ ngay hoặc đổi cài đặt.

GitHub chỉ nhận TLS 1.2, còn trình duyệt gốc của S60 chỉ có TLS 1.0. Muốn tải trực
tiếp trên điện thoại thì cài [bản vá TLS](https://nnproject.cc/tls/), hoặc tải bằng
máy tính rồi chép sang điện thoại.

Lần đồng bộ đầu tiên ngay sau khi cài trông như sau:

```
2026-10-22 22:31:04  ntpsyncd 2.0.3 started by hand
2026-10-22 22:31:04  Sync (started by hand)
2026-10-22 22:31:04  Asking which Wi-Fi access point to use
2026-10-22 22:31:12  Wi-Fi access point 13 (HomeWifi) picked
2026-10-22 22:31:12  Saved access point 13 to C:\Data\ntpsyncd.ini
2026-10-22 22:31:16  Clock corrected by -214.381 s - vn.pool.ntp.org (203.0.113.7), stratum 2, 47 ms round trip
2026-10-22 22:31:16  Next sync around 04:31
```

### Cần biết

- **Cập nhật:** nếu trình cài báo ntpsyncd đang chạy, vào app chọn Tắt chạy nền
  trước, cài xong chọn Bật chạy nền.
- **Gỡ cài đặt:** chọn Tắt chạy nền trước, nếu không trình cài có thể báo ntpsyncd
  đang chạy. File `C:\Data\ntpsyncd.ini` vẫn được giữ lại.
- **Riêng tư:** chỉ liên lạc với các server NTP trong cài đặt (cổng UDP 123).
  Nhật ký nằm trên máy.

### Vì sao cần máy đã hack

Từ S60 3rd Edition, mỗi chương trình phải khai báo các quyền (capability) nó cần.
Trình cài đặt chỉ cấp quyền hệ thống cho gói được ký qua Symbian Signed; gói tự ký
chỉ có quyền người dùng, như dùng mạng.

| Quyền | Loại | Dùng để |
|---|---|---|
| `WriteDeviceData` | hệ thống | Đặt giờ bằng `User::SetUTCTime`, cách duy nhất để chỉnh đồng hồ trên Symbian. Thiếu quyền này, app chỉ đo được độ lệch mà không chỉnh được |
| `ReadDeviceData` | hệ thống | Đọc danh sách điểm truy cập: lọc ra Wi-Fi, hiện danh sách đánh dấu trong Cài đặt và tên Wi-Fi trên màn hình trạng thái |
| `NetworkServices` | người dùng | Gửi gói NTP. Gói tự ký có quyền này bình thường |

Symbian Signed, kể cả bản miễn phí khóa theo IMEI từng máy, đã ngừng hoạt động, nên
không còn cách hợp lệ nào để có chữ ký mang quyền hệ thống. "Hack" ở đây chỉ là bản
vá InstallServer: trình cài bỏ qua việc kiểm tra chữ ký và cấp đủ quyền cho gói tự
ký. Không cần Open4All hay bản vá nào khác.

## File cài đặt (`C:\Data\ntpsyncd.ini`)

| Khóa | Mặc định | Ý nghĩa |
|---|---|---|
| `server=` | pool vn / asia / toàn cầu | Các server NTP, thử lần lượt, tối đa 5 dòng |
| `iap=` | không có | ID các điểm truy cập Wi-Fi dịch vụ được tự bật, mỗi dòng một cái. Chọn trong danh sách sẽ thêm vào đây. ID không phải Wi-Fi trên máy này (dữ liệu di động, hoặc đã bị xóa) và Easy WLAN bị bỏ qua, có ghi trong nhật ký. Không có cái nào dùng được thì đồng bộ theo lịch chỉ chạy khi Wi-Fi đang bật sẵn, 10 phút kiểm tra một lần cho tới khi thành công |
| `interval=` | 360 | Số phút giữa hai lần đồng bộ (15 tới 10080) |
| `threshold=` | 500 | Để yên đồng hồ nếu lệch ít hơn số mili giây này |
| `watch=` | 1 | Đồng bộ lại khi thứ khác đổi giờ máy |
| `enabled=` | 1 | 0 thì dịch vụ dừng và không chạy khi bật máy. Do Tắt / Bật chạy nền trong app đặt |

Màn hình Cài đặt của app chỉnh được tất cả những mục này (chu kỳ và ngưỡng là
danh sách các giá trị hay dùng). Lưu cài đặt trong app sẽ viết lại cả file theo
mẫu chuẩn, nên các chú thích bạn tự thêm sẽ mất. File liệt kê mọi điểm truy cập
Wi-Fi, cái không dùng thành dòng chú thích (`;iap=3    ; HomeWifi`); bỏ dấu `;` để
bật. Sửa tay thì có hiệu lực từ lần đồng bộ sau, hoặc ngay khi chọn **Đồng bộ ngay**.

Đồng bộ thất bại thì 10 phút sau thử lại, rồi giãn dần 20, 40, 80… phút (không
quá chu kỳ) nếu có điểm truy cập để tự bật.

## Xử lý sự cố

Các dòng nhật ký được ghi bằng tiếng Anh:

- **"Could not set the clock (-46)… WriteDeviceData capability missing"**: gói cài
  mất quyền. Ký lại và cài trên máy đã vá InstallServer.
- **"No Wi-Fi connection is up and no iap= is enabled"**: đánh dấu Wi-Fi của bạn ở
  Cài đặt → Điểm truy cập Wi-Fi, hoặc chọn **Đồng bộ ngay** rồi chọn trong danh sách.
- **"No access point picked (-3)"**: danh sách bị hủy. Không có gì được lưu; lần
  **Đồng bộ ngay** sau nó hỏi lại.
- **"Settings: iap=N is not a Wi-Fi access point on this phone, ignored"**: điểm
  truy cập đó đã bị xóa (tạo lại, ví dụ sau khi đổi mật khẩu Wi-Fi, sẽ có ID mới)
  hoặc không phải Wi-Fi. Đánh dấu đúng cái ở Cài đặt → Điểm truy cập Wi-Fi; lưu ở
  đó sẽ bỏ dòng cũ.
- **"… did not connect (n)"**: điểm truy cập đó ngoài vùng phủ sóng hoặc đã đổi mật
  khẩu. Nó thử cái tiếp theo.
- **"No reply from …"**: mạng đó chặn cổng UDP 123, hoặc server không chạy. Thêm
  server khác.
- **"Clock moved by +X s by something else"**: chương trình khác đã đặt giờ. Độ
  nhảy được đo so với bộ đếm nhịp; "N s after ntpsyncd last set it" giúp biết có
  thứ gì đang hoàn tác lần chỉnh của nó không.
- **"Clock moved by +X s with no time-change notice"**: giờ nhảy mà hệ thống không
  báo, do lần kiểm tra 5 phút một lần phát hiện.
- **"Clock reads … off right after it was set"**: máy không giữ đúng giờ vừa được
  đặt (ví dụ làm tròn thành giây chẵn).
- **"UTC offset changed from A to B min"**: múi giờ hoặc giờ mùa hè thay đổi.
- **Không có gì trong nhật ký sau khi khởi động lại**: mục tự chạy không được nhận,
  hoặc chạy nền đang tắt (nhật ký ghi `Background sync is off`). Chọn Bật chạy nền
  trong app, và kiểm tra có file `c:\private\101f875a\import\[E7A15D01].rsc`.
- **App báo "Đã tắt" dù bạn không tắt**: dịch vụ không chạy. Bật chạy nền sẽ chạy
  lại; nhật ký cho biết vì sao nó dừng.
- **Đồng bộ ngay không làm gì**: đang có một lần đồng bộ chạy (màn hình trạng thái
  có ghi). Chạy `c:\sys\bin\ntpsyncd.exe` từ X-plore khi dịch vụ đang chạy cũng
  có nghĩa là "đồng bộ ngay".

Muốn nó không phản ứng khi thứ khác đổi giờ, tắt Cài đặt → Theo dõi đổi giờ
(`watch=0`).

## Build từ mã nguồn

Việc build chạy trong Docker image công khai
[`minhkhoi4dev/s60v3-sdk-docker`](https://hub.docker.com/r/minhkhoi4dev/s60v3-sdk-docker)
(SDK S60 3rd Edition FP2, GnuPoc, GCCE), trên Mac, Linux hoặc Windows (WSL):

```sh
bash group/docker-build.sh   # -> sis/ntpsyncd.sisx
```

Script chép dự án vào container rồi mới build, vì bộ công cụ GCCE 32-bit lỗi trên
ổ Windows gắn vào WSL ("Value too large for defined data type"). Trên Windows,
chạy từ WSL: `wsl -- bash /mnt/c/duong/dan/ntp-time-sync-s60/group/docker-build.sh`.
Đặt `NTPSYNCD_SDK_IMAGE` để dùng image khác.

Hoặc dùng SDK GnuPoc trên máy:

```sh
export EPOCROOT=/path/to/s60_sdk/   # bắt buộc có dấu / ở cuối
group/build.sh                       # bldmake, abld build gcce urel, makesis, signsis
```

`build.sh` kiểm tra cả hai file exe và tài nguyên của app thật sự được build (abld
báo thành công cả khi có file lỗi), đưa `gfx/ntpsyncui.svg` vào `ntpsyncui_aif.mif`
bằng `mifconv` của GnuPoc (giữ nguyên SVG; máy tự đọc), và thay `$(EPOCROOT)` trong
`sis/ntpsyncd.pkg` (ghi ra `sis/build.pkg`) trước khi chạy `makesis`. Lần build đầu
tạo cặp khóa tự ký `sis/dev.key` và `sis/dev.cer`, các lần sau dùng lại.

Phần tính toán NTP có kiểm thử chạy trên máy tính (offset và độ trễ, mốc chuyển
kỷ nguyên NTP năm 2036, đồng hồ máy bị reset về 2009, và mọi quy tắc loại gói tin):

```sh
g++ -std=c++98 -Wall -Wextra -Itest/shim -Iinc test/test_ntppacket.cpp src/ntppacket.cpp -o /tmp/t && /tmp/t
```

GitHub Actions (`.github/workflows/build.yml`) chạy kiểm thử và build bằng Docker
mỗi lần push thay đổi code. Phát hành một phiên bản: đặt số phiên bản trong
`sis/ntpsyncd.pkg` và trong `KVersion` ở `src/daemon.cpp` (bản build kiểm tra hai
chỗ khớp nhau), rồi push tag `v<phiên bản>` (ví dụ `v2.0.3`). Actions tạo release
kèm `ntp-time-sync-<phiên bản>.sisx` và cập nhật trang giới thiệu (`site/`,
`.github/workflows/pages.yml`). Có secret `NTPSYNCD_DEV_KEY` và `NTPSYNCD_DEV_CER`
thì ký bằng khóa đó; không có thì mỗi lần chạy tạo khóa tạm, máy đã hack vẫn nhận.

## Mã nguồn

| Thư mục | Nội dung |
|---------|----------|
| `src` | Dịch vụ: lịch đồng bộ và theo dõi đổi giờ (`daemon.cpp`), kết nối Wi-Fi và trao đổi UDP (`syncsession.cpp`), gói tin SNTP và phép tính offset (`ntppacket.cpp`), file cài đặt (`config.cpp`), nhật ký (`logger.cpp`) |
| `src/ui` | Ứng dụng NTP Time Sync: màn hình trạng thái, danh sách cài đặt, xem log |
| `inc` | Header; `ntpsyncd.hrh` chứa UID và khóa Publish & Subscribe, `ntpshared.h` là những gì dịch vụ công bố và app được gửi |
| `data` | Mục tự chạy khi bật máy; tài nguyên, đăng ký và chữ trên màn hình của app (`ntpsyncui_vi.loc`, `ntpsyncui_en.loc`) |
| `gfx` | Icon của app (SVG Tiny, `build.sh` đưa vào file MIF) |
| `group` | File build (`.mmp`), script build |
| `sis` | `ntpsyncd.pkg` |
| `test` | Kiểm thử phần tính toán NTP, chạy bằng trình biên dịch của máy tính |
| `site` | Trang giới thiệu và bản nhẹ cho điện thoại (`site/m/`) |

Dịch vụ và app nói chuyện qua Publish & Subscribe trong category của dịch vụ: dịch
vụ ghi trạng thái (khóa 2), app gửi lệnh (khóa 3: đồng bộ ngay, đọc lại cài đặt,
dừng), và chỉ SID của app được ghi khóa đó. UID `0xE7A15D01` (dịch vụ, gói cài) và
`0xE7A15D02` (app) nằm trong dải tự ký.

Code C++98 cho GCCE 3.4.3: không STL, không exception.

---

Dự án cá nhân của [Khoi Nguyen](https://github.com/minhkhoi-dev). Không liên quan đến Nokia,
HMD Global hay NTP Pool Project.
