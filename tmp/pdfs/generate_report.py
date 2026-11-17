from pathlib import Path
import shutil
from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER, TA_LEFT
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.units import mm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (
    SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle, PageBreak,
    KeepTogether, Flowable
)

ROOT = Path(__file__).resolve().parents[2]
OUT_DIR = ROOT / "output" / "pdf"
OUT_DIR.mkdir(parents=True, exist_ok=True)
OUT = OUT_DIR / "Lab01_Report.pdf"
SUBMISSION_COPY = ROOT / "REPORT.pdf"

FONT_DIR = Path(r"C:\Windows\Fonts")
pdfmetrics.registerFont(TTFont("Arial", str(FONT_DIR / "arial.ttf")))
pdfmetrics.registerFont(TTFont("Arial-Bold", str(FONT_DIR / "arialbd.ttf")))
pdfmetrics.registerFont(TTFont("Arial-Italic", str(FONT_DIR / "ariali.ttf")))
pdfmetrics.registerFontFamily("Arial", normal="Arial", bold="Arial-Bold", italic="Arial-Italic", boldItalic="Arial-Bold")

NAVY = colors.HexColor("#17324D")
BLUE = colors.HexColor("#2878A5")
TEAL = colors.HexColor("#2C8C87")
PALE = colors.HexColor("#EEF4F7")
PALE_BLUE = colors.HexColor("#EAF2F8")
INK = colors.HexColor("#25313B")
MUTED = colors.HexColor("#647481")
GREEN = colors.HexColor("#2F7558")
AMBER = colors.HexColor("#98631C")
RED = colors.HexColor("#9F4545")

styles = getSampleStyleSheet()
styles.add(ParagraphStyle(name="BodyVI", fontName="Arial", fontSize=9.2, leading=13.2, textColor=INK, spaceAfter=5))
styles.add(ParagraphStyle(name="SmallVI", fontName="Arial", fontSize=8, leading=10.5, textColor=INK, spaceAfter=2))
styles.add(ParagraphStyle(name="TinyVI", fontName="Arial", fontSize=7.1, leading=9, textColor=INK))
styles.add(ParagraphStyle(name="TableHeadVI", fontName="Arial-Bold", fontSize=7.4, leading=9.2, textColor=colors.white))
styles.add(ParagraphStyle(name="TitleVI", fontName="Arial-Bold", fontSize=25, leading=30, alignment=TA_CENTER, textColor=NAVY, spaceAfter=8))
styles.add(ParagraphStyle(name="SubTitleVI", fontName="Arial", fontSize=13, leading=18, alignment=TA_CENTER, textColor=BLUE, spaceAfter=8))
styles.add(ParagraphStyle(name="H1VI", fontName="Arial-Bold", fontSize=16, leading=20, textColor=NAVY, spaceBefore=1, spaceAfter=10))
styles.add(ParagraphStyle(name="H2VI", fontName="Arial-Bold", fontSize=11, leading=14, textColor=BLUE, spaceBefore=7, spaceAfter=4))
styles.add(ParagraphStyle(name="CenterVI", fontName="Arial", fontSize=9, leading=13, alignment=TA_CENTER, textColor=INK))
styles.add(ParagraphStyle(name="CoverMeta", fontName="Arial", fontSize=10, leading=15, alignment=TA_CENTER, textColor=MUTED))
styles.add(ParagraphStyle(name="CalloutVI", fontName="Arial", fontSize=8.6, leading=12, textColor=NAVY, backColor=PALE_BLUE, borderColor=BLUE, borderWidth=0.6, borderPadding=7, spaceBefore=3, spaceAfter=6))

def P(text, style="BodyVI"):
    return Paragraph(text, styles[style])

def header_footer(canvas, doc):
    canvas.saveState()
    width, height = A4
    if doc.page > 1:
        canvas.setStrokeColor(colors.HexColor("#D8E1E7"))
        canvas.setLineWidth(0.6)
        canvas.line(18*mm, height-16*mm, width-18*mm, height-16*mm)
        canvas.setFont("Arial-Bold", 7.5)
        canvas.setFillColor(NAVY)
        canvas.drawString(18*mm, height-12.5*mm, "LAB 01  |  BLOCKCHAIN LAYER 1 SIMULATOR")
        canvas.setFont("Arial", 7.5)
        canvas.setFillColor(MUTED)
        canvas.drawRightString(width-18*mm, height-12.5*mm, "Báo cáo kỹ thuật")
    canvas.setStrokeColor(colors.HexColor("#D8E1E7"))
    canvas.line(18*mm, 15*mm, width-18*mm, 15*mm)
    canvas.setFont("Arial", 7.5)
    canvas.setFillColor(MUTED)
    canvas.drawString(18*mm, 10.5*mm, "Blockchain course • Khoa Công nghệ Tri thức")
    canvas.drawRightString(width-18*mm, 10.5*mm, f"Trang {doc.page}")
    canvas.restoreState()

class ArchitectureDiagram(Flowable):
    def __init__(self, width=168*mm, height=65*mm):
        super().__init__()
        self.width, self.height = width, height

    def draw(self):
        c = self.canv
        boxes = [
            (2, 35, 51, 22, "Giao dịch", "TX:lab01"),
            (60, 35, 51, 22, "State + Block", "SHA-256 / state root"),
            (118, 35, 51, 22, "Consensus", "PREVOTE → PRECOMMIT"),
        ]
        for idx, (x, y, w, h, title, detail) in enumerate(boxes):
            c.setFillColor(PALE_BLUE if idx != 2 else colors.HexColor("#E8F4F1"))
            c.setStrokeColor(BLUE if idx != 2 else TEAL)
            c.roundRect(x*mm, y*mm, w*mm, h*mm, 3*mm, fill=1, stroke=1)
            c.setFillColor(NAVY)
            c.setFont("Arial-Bold", 8.4)
            c.drawCentredString((x+w/2)*mm, (y+13)*mm, title)
            c.setFillColor(MUTED)
            c.setFont("Arial", 6.6)
            c.drawCentredString((x+w/2)*mm, (y+6)*mm, detail)
        c.setStrokeColor(BLUE)
        c.setFillColor(BLUE)
        c.setLineWidth(1.3)
        for x1, x2 in [(53, 60), (111, 118)]:
            c.line(x1*mm, 46*mm, x2*mm, 46*mm)
            c.line(x2*mm, 46*mm, (x2-2)*mm, 47.5*mm)
            c.line(x2*mm, 46*mm, (x2-2)*mm, 44.5*mm)
        # Network and local validation nodes under the consensus block.
        c.setFillColor(PALE)
        c.setStrokeColor(colors.HexColor("#9FB2BF"))
        c.roundRect(34*mm, 3*mm, 101*mm, 19*mm, 3*mm, fill=1, stroke=1)
        c.setFillColor(NAVY)
        c.setFont("Arial-Bold", 8.5)
        c.drawCentredString(84.5*mm, 15*mm, "Network mô phỏng  •  các validator giữ view cục bộ")
        c.setFont("Arial", 6.8)
        c.setFillColor(MUTED)
        c.drawCentredString(84.5*mm, 8.5*mm, "delay / drop / duplicate / block  →  chữ ký vote  →  quorum 2f+1")
        c.setStrokeColor(TEAL)
        c.line(143.5*mm, 35*mm, 143.5*mm, 26*mm)
        c.line(143.5*mm, 26*mm, 135*mm, 19*mm)
        c.line(143.5*mm, 26*mm, 135*mm, 26*mm)

def section(title, lead=None):
    out = [P(title, "H1VI")]
    if lead:
        out.append(P(lead))
    return out

story = []

# Cover
story += [Spacer(1, 20*mm), P("BÁO CÁO LAB 01", "TitleVI"),
          P("Blockchain Layer 1 Simulator", "SubTitleVI"), Spacer(1, 4*mm)]
cover_meta = Table([
    [P("HỌC PHẦN", "SmallVI"), P("Blockchain và Ứng dụng", "SmallVI")],
    [P("ĐƠN VỊ", "SmallVI"), P("Khoa Công nghệ Tri thức", "SmallVI")],
    [P("NĂM HỌC", "SmallVI"), P("2026", "SmallVI")],
], colWidths=[34*mm, 95*mm], hAlign="CENTER")
cover_meta.setStyle(TableStyle([
    ("BACKGROUND", (0,0), (0,-1), PALE_BLUE), ("TEXTCOLOR", (0,0), (0,-1), NAVY),
    ("FONTNAME", (0,0), (-1,-1), "Arial"), ("VALIGN", (0,0), (-1,-1), "MIDDLE"),
    ("LEFTPADDING", (0,0), (-1,-1), 8), ("RIGHTPADDING", (0,0), (-1,-1), 8),
    ("TOPPADDING", (0,0), (-1,-1), 7), ("BOTTOMPADDING", (0,0), (-1,-1), 7),
    ("BOX", (0,0), (-1,-1), 0.6, colors.HexColor("#CBD8E0")),
    ("INNERGRID", (0,0), (-1,-1), 0.35, colors.HexColor("#D7E0E6")),
]))
story += [cover_meta, Spacer(1, 12*mm), P("THÀNH VIÊN NHÓM", "H2VI")]
team = [[P("STT", "TableHeadVI"), P("Họ và tên", "TableHeadVI"), P("MSSV", "TableHeadVI"), P("Phần việc", "TableHeadVI")]]
members = [
    ("1", "Lê Mạnh Phú", "23120325", "T1–T5"),
    ("2", "Vũ Hoàng Long", "23120058", "T6–T8"),
]
for member in members:
    team.append([P(value, "TinyVI") for value in member])
team_table = Table(team, colWidths=[13*mm, 62*mm, 39*mm, 55*mm], rowHeights=[8*mm]+[11*mm]*len(members), repeatRows=1)
team_table.setStyle(TableStyle([
    ("BACKGROUND", (0,0), (-1,0), NAVY), ("TEXTCOLOR", (0,0), (-1,0), colors.white),
    ("FONTNAME", (0,0), (-1,0), "Arial-Bold"), ("FONTNAME", (0,1), (-1,-1), "Arial"),
    ("VALIGN", (0,0), (-1,-1), "MIDDLE"), ("ALIGN", (0,0), (0,-1), "CENTER"),
    ("GRID", (0,0), (-1,-1), 0.45, colors.HexColor("#BCCBD5")),
    ("ROWBACKGROUNDS", (0,1), (-1,-1), [colors.white, PALE]),
    ("LEFTPADDING", (0,0), (-1,-1), 6), ("RIGHTPADDING", (0,0), (-1,-1), 6),
]))
story += [team_table, Spacer(1, 12*mm),
          P("Tóm tắt", "H2VI"),
          P("Lab xây dựng mô phỏng blockchain Layer 1 bằng C++20: giao dịch và vote được ký Ed25519, trạng thái key–value được thực thi xác định, block cam kết state root, còn network mô phỏng delay, drop, duplicate và chặn peer. Bộ kiểm thử bao phủ các kịch bản T1–T8; báo cáo nêu rõ phần đã chạy được và những giới hạn còn lại so với đề bài."),
          Spacer(1, 8*mm), P("Bản báo cáo kỹ thuật • Thông tin thành viên theo phân công nhóm", "CoverMeta"), PageBreak()]

# Page 2 architecture and flow
story += section("1. Mục tiêu và thiết kế hệ thống", "Mục tiêu của lab là mô phỏng cách các node xác thực dữ liệu, trao đổi vote qua mạng không tin cậy và đạt finality khi có đủ quorum. Mỗi node xử lý thông điệp trong view cục bộ; mô hình không dùng bộ nhớ chia sẻ giữa các validator.")
story += [P("1.1. Sơ đồ kiến trúc", "H2VI"), ArchitectureDiagram(),
          P("Luồng chính: người gửi ký transaction → State kiểm tra chữ ký, quyền sở hữu key và nonce → Block tính state root và ký header → validator gửi vote qua Network → mỗi node tự đếm PREVOTE/PRECOMMIT và finalize khi có quorum.", "SmallVI"),
          P("1.2. Các module", "H2VI")]
modules = [
    ["Module", "Trách nhiệm"],
    ["crypto / transaction", "Ed25519, SHA-256, mã hóa hex, domain separation cho TX và header."],
    ["state / block", "Áp dụng giao dịch có thứ tự; tính state root; xác thực parent, height, chữ ký header và transaction trùng."],
    ["validator / vote / consensus", "Tạo validator set xác định, round-robin proposer, vote hai pha, quorum 2f+1, lock và phát hiện equivocation."],
    ["network / distributed_node", "Hàng đợi message theo delivery tick; gửi vote ký số, loại duplicate và theo dõi finality cục bộ."],
    ["logger / lab01", "Ghi sự kiện demo ra logs/run.log; chạy demo transaction → state → block."],
]
mt = Table([[P(x, "TableHeadVI" if ri == 0 else "TinyVI") for x in row] for ri, row in enumerate(modules)], colWidths=[42*mm, 127*mm], repeatRows=1)
mt.setStyle(TableStyle([
    ("BACKGROUND", (0,0), (-1,0), NAVY), ("TEXTCOLOR", (0,0), (-1,0), colors.white),
    ("FONTNAME", (0,0), (-1,0), "Arial-Bold"), ("VALIGN", (0,0), (-1,-1), "TOP"),
    ("GRID", (0,0), (-1,-1), 0.4, colors.HexColor("#CBD6DE")),
    ("ROWBACKGROUNDS", (0,1), (-1,-1), [colors.white, PALE]),
    ("LEFTPADDING", (0,0), (-1,-1), 6), ("RIGHTPADDING", (0,0), (-1,-1), 6),
    ("TOPPADDING", (0,0), (-1,-1), 5), ("BOTTOMPADDING", (0,0), (-1,-1), 5),
]))
story += [mt, P("1.3. Ranh giới trust", "H2VI"),
          P("Validator có key riêng; thông điệp consensus được xác minh bằng public key của signer. Các node kiểm tra transport sender trùng validator_id trong vote. Random seed chỉ giúp tái lập mô phỏng, không phải nguồn entropy an toàn để sinh khóa production.") , PageBreak()]

# Page 3 implementation details
story += section("2. Ghi chú triển khai", "Mã nguồn dùng C++20 và OpenSSL 3 Crypto. Các chuỗi được ghép theo quy tắc có độ dài tiền tố ở nhiều payload để tránh nhập nhằng giữa các trường.")
story += [P("2.1. Mật mã và dữ liệu xác định", "H2VI"),
          P("crypto::keyPairFromSeed nhận seed Ed25519 32 byte và dẫn xuất public key qua EVP. crypto::sign / verify ký payload có prefix domain và độ dài message. Transaction dùng TX:lab01; block header dùng HEADER:lab01; vote dùng VOTE:&lt;chain_id&gt;. SHA-256 được dùng cho state root, transaction ID, Merkle root giao dịch và block hash."),
          P("Transaction payload bao gồm sender, key, value, nonce theo thứ tự cố định. State yêu cầu key nằm trong namespace sender/..., nonce bắt đầu từ 0 và tăng tuần tự. State lưu map có thứ tự; root được băm từ key/value và nonce theo thứ tự xác định."),
          P("2.2. Block và thực thi", "H2VI"),
          P("Block header ký height, parent hash, state root, proposer và root của danh sách transaction. Transaction root dùng cây nhị phân: hash các tx.id(), ghép cặp trái–phải, nếu tầng có số nút lẻ thì lặp lại nút cuối. Block::validateBlock thực thi transaction trên bản sao của previousState, từ chối transaction ID trùng và so state root tính lại với state_root được cam kết."),
          P("2.3. Network mô phỏng", "H2VI"),
          P("Network đặt message vào priority queue theo delivery_tick rồi sequence để giữ thứ tự ổn định khi cùng tick. Cấu hình có min/max delay, seed, xác suất duplicate/drop và danh sách scripted_delays cho test có lịch giao cụ thể. setPeerBlocked chặn một cặp sender/receiver. Logger ghi send, delay, drop, duplicate, delivery, block và unblock khi được truyền vào Network."),
          P("2.4. Consensus và vote", "H2VI"),
          P("ValidatorSet yêu cầu n = 3f + 1; quorum là 2f + 1. ConsensusEngine giữ phiếu theo height, round và phase, chống double-vote cục bộ, xác minh chữ ký và áp dụng lock/valid_round. DistributedValidatorNode ghi nhận vote theo round/phase; khi cùng validator ký hai block hash trong cùng khóa vote, node xóa vote đầu và đưa validator vào tập equivocators để không phiếu nào của họ góp quorum."),
          P("T1–T8 tập trung vào một height của simulator. onRoundTimeout() là điểm gọi từ scheduler: scheduler cung cấp hash ứng viên vòng kế tiếp, node tăng round và phát prevote mới. Bản hiện tại chưa có bộ đếm timeout nội bộ hay module tự chọn proposer và tạo block.", "CalloutVI"), PageBreak()]

# Page 4 flow and safety, limitations
story += section("3. Bảo vệ an toàn và xử lý lỗi", "Các guard được thể hiện trong code và test; phần này phân biệt rõ thuộc tính đã kiểm tra với những bước chưa được tích hợp xuyên suốt toàn hệ thống.")
story += [P("3.1. Các guard chính", "H2VI")]
guards = [
    ["Rủi ro", "Cách xử lý"],
    ["Giả mạo / sai domain", "Chữ ký được verify với public key và domain theo loại thông điệp; test sai chữ ký và chain id bị từ chối."],
    ["Transaction ngoài quyền sở hữu", "State chỉ chấp nhận key thuộc tiền tố sender/; nonce sai hoặc lặp trả invalid_or_replayed_nonce."],
    ["Block sai dữ liệu", "Đối chiếu height, parent hash, chữ ký header, transaction ID trùng và state root sau khi replay."],
    ["Vote lặp", "Vote lưu theo validator ID; bản lặp không làm tăng số phiếu; quorum đếm mỗi validator tối đa một lần."],
    ["Equivocation", "Gỡ phiếu đầu khỏi map, ghi dấu equivocator trong cặp (round, phase), bỏ qua các vote tiếp theo ở cặp đó."],
    ["Message delay/drop", "Priority queue và seeded RNG tạo trễ có thể tái lập; test kiểm tra an toàn trước giao message và hội tụ sau giao message."],
]
gt = Table([[P(c, "TableHeadVI" if ri == 0 else "TinyVI") for c in row] for ri, row in enumerate(guards)], colWidths=[46*mm, 123*mm], repeatRows=1)
gt.setStyle(TableStyle([
    ("BACKGROUND", (0,0), (-1,0), NAVY), ("TEXTCOLOR", (0,0), (-1,0), colors.white),
    ("FONTNAME", (0,0), (-1,0), "Arial-Bold"), ("VALIGN", (0,0), (-1,-1), "TOP"),
    ("GRID", (0,0), (-1,-1), 0.4, colors.HexColor("#CBD6DE")),
    ("ROWBACKGROUNDS", (0,1), (-1,-1), [colors.white, PALE]),
    ("LEFTPADDING", (0,0), (-1,-1), 6), ("RIGHTPADDING", (0,0), (-1,-1), 6),
    ("TOPPADDING", (0,0), (-1,-1), 5), ("BOTTOMPADDING", (0,0), (-1,-1), 5),
]))
story += [gt, P("3.2. Hạn chế cần hiểu khi diễn giải kết quả", "H2VI"),
          P("DistributedValidatorNode xác nhận vote theo height hiện tại và test E2E dùng 4 validator. Test T6 gọi onRoundTimeout từ harness; chưa kiểm tra đồng hồ timeout tự phát hay tự chọn proposer. Test T7 kiểm tra f=1. Mã nguồn chưa ghép việc kiểm tra block hợp lệ/proposer vào đường nhận vote phân tán, chưa truyền header trước body, chưa giới hạn throughput, chưa có persistence/WAL, RPC hay đồng bộ history nhiều height."),
          P("Vì vậy, kết quả xác nhận các thuộc tính được test trong mô phỏng, không chứng minh toàn bộ yêu cầu về node production hoặc khả năng phục hồi chuỗi nhiều block. Tài liệu đề bài yêu cầu simulator hỗ trợ ít nhất 8 node; bộ E2E hiện tại chưa có case 8 node.", "CalloutVI"), PageBreak()]

# Page 5 test matrix
story += section("4. Kiểm thử và kết quả", "Đã biên dịch trực tiếp các executable test bằng GCC C++20, sau đó chạy tuần tự toàn bộ test hiện có và test bổ sung. Tất cả executable trả mã thoát 0; lệnh CTest chưa được dùng do CMake chưa có sẵn trong PATH của môi trường kiểm thử.")
test_rows = [
    ["ID", "Kịch bản", "Kết quả quan sát"],
    ["T1", "Chạy bình thường, không fault", "4 validator finalize cùng block trong distributed_tests."],
    ["T2", "Reorder và duplicate message", "Thứ tự B,C,A được tái hiện; duplicate vote không nhân đôi quorum."],
    ["T3", "Chữ ký sai / chain sai", "Vote bị từ chối; không khóa hoặc finalize block."],
    ["T4", "Replay transaction", "Transaction lặp hoặc nonce cũ bị từ chối; state không bị đổi."],
    ["T5", "Drop/delay message", "Không finalize xung đột khi thiếu quorum; giao message trễ giúp các node hội tụ."],
    ["T6", "Proposer im lặng", "Harness báo timeout; 3 validator còn lại chuyển sang round kế tiếp và finalize cùng block."],
    ["T7", "Byzantine equivocate (f=1)", "Vote xung đột của validator bị loại; các validator đúng không chia finality."],
    ["T8", "Chạy lại cùng seed", "Hai lần chạy có stdout, state hash và logs/run.log giống hệt."],
]
tt = Table([[P(c, "TableHeadVI" if ri == 0 else "TinyVI") for c in row] for ri, row in enumerate(test_rows)], colWidths=[14*mm, 50*mm, 105*mm], repeatRows=1)
tt.setStyle(TableStyle([
    ("BACKGROUND", (0,0), (-1,0), NAVY), ("TEXTCOLOR", (0,0), (-1,0), colors.white),
    ("FONTNAME", (0,0), (-1,0), "Arial-Bold"), ("VALIGN", (0,0), (-1,-1), "TOP"),
    ("ALIGN", (0,1), (0,-1), "CENTER"), ("GRID", (0,0), (-1,-1), 0.4, colors.HexColor("#CBD6DE")),
    ("ROWBACKGROUNDS", (0,1), (-1,-1), [colors.white, PALE]),
    ("LEFTPADDING", (0,0), (-1,-1), 5), ("RIGHTPADDING", (0,0), (-1,-1), 5),
    ("TOPPADDING", (0,0), (-1,-1), 5), ("BOTTOMPADDING", (0,0), (-1,-1), 5),
]))
story += [tt, P("Kết quả định danh của demo T8", "H2VI"),
          P("State root: <font name='Arial-Bold'>c9c73ee6330beb8895328fc9046eb56909329f2dee32c23e28fb95c0b3e69c5c</font><br/>Block hash: <font name='Arial-Bold'>06ca333ed0e8890bcc525ed583a49f24285eb10302747cb926874d336570b92e</font>", "SmallVI"),
          P("Nhận xét", "H2VI"),
          P("Các test unit xác nhận chữ ký/domain, state, block validation, cấu hình validator và network. T1–T5 kiểm tra propagation và fault trong distributed tests. T6 chứng minh đường chuyển round do harness kích hoạt; T7 chứng minh một equivocator không tạo được quorum xung đột trong kịch bản đã chạy; T8 so sánh byte log bằng SHA-256 và so sánh toàn bộ stdout."), PageBreak()]

# Page 6 run and references
story += section("5. Cách build, chạy và tài liệu tham khảo", "Các bước dưới đây lấy từ README của repository và dùng Developer PowerShell for VS 2022 trên Windows.")
story += [P("5.1. Build và chạy", "H2VI"),
          P("1. Cài Visual Studio 2022 workload Desktop development with C++, CMake 3.20+ và OpenSSL 3.x development package qua vcpkg.<br/>2. Từ thư mục dự án, cấu hình và build:", "BodyVI")]
code = Table([[P("cmake -S . -B build -A x64 -DCMAKE_TOOLCHAIN_FILE=C:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake<br/>cmake --build build --config Release<br/>ctest --test-dir build -C Release --output-on-failure", "SmallVI")]], colWidths=[168*mm])
code.setStyle(TableStyle([("BACKGROUND", (0,0), (-1,-1), colors.HexColor("#F1F4F6")), ("BOX", (0,0), (-1,-1), 0.5, colors.HexColor("#CFD9DF")), ("LEFTPADDING", (0,0), (-1,-1), 8), ("RIGHTPADDING", (0,0), (-1,-1), 8), ("TOPPADDING", (0,0), (-1,-1), 7), ("BOTTOMPADDING", (0,0), (-1,-1), 7)]))
story += [code, Spacer(1, 4), P("Chạy demo bằng .\\build\\Release\\lab01.exe. T8 có thể chạy riêng bằng .\\scripts\\check_determinism.ps1 -Executable .\\build\\Release\\lab01.exe. Script gọi demo hai lần, chụp logs/run.log sau từng lượt và so sánh kết quả."),
          P("5.2. Cấu trúc repository", "H2VI"),
          P("include/ chứa khai báo API; src/ chứa crypto, transaction, state, block, network, validator, vote, consensus, distributed node, logger và demo; tests/ chứa unit, consensus, distributed, replay, network fault và completion tests; config/default.json mô tả chain/network mẫu; logs/run.log là log demo; scripts/check_determinism.ps1 kiểm tra T8."),
          P("5.3. Vấn đề kỹ thuật đáng chú ý", "H2VI"),
          P("Replay được chặn bằng nonce kỳ tự tăng và vì transaction ID trùng không được chấp nhận trong block. Khi phát hiện equivocation, chỉ bỏ phiếu thứ hai là chưa đủ vì phiếu thứ nhất vẫn có thể góp quorum; implementation xóa phiếu đã lưu và đánh dấu signer là equivocator trong phase/round đó. Message cùng delivery tick được ổn định thứ tự bằng sequence, tránh kết quả phụ thuộc vào thứ tự nội bộ priority queue."),
          P("5.4. Tài liệu và liên kết", "H2VI"),
          P("• Đề bài: <i>Lab01.pdf</i>, Blockchain course, Knowledge Engineering Department, 2026.<br/>• Bài giảng: <i>Lab01-presentation.pdf</i>, “Node Architecture, Clients, and State Synchronization”, 2025.<br/>• OpenSSL 3, EVP Ed25519: https://docs.openssl.org/3.0/man7/EVP_SIGNATURE-ED25519/ .<br/>• CMake Reference Documentation: https://cmake.org/documentation/ .<br/>• Không sử dụng sách ngoài trong phần triển khai.", "SmallVI"),
          Spacer(1, 5), P("— Hết —", "CenterVI")]

doc = SimpleDocTemplate(str(OUT), pagesize=A4, rightMargin=18*mm, leftMargin=18*mm,
                        topMargin=21*mm, bottomMargin=20*mm,
                        title="Báo cáo Lab01 - Blockchain Layer 1 Simulator",
                        author="Nhóm Lab01", subject="Báo cáo triển khai và kiểm thử Lab01")
doc.build(story, onFirstPage=header_footer, onLaterPages=header_footer)
shutil.copy2(OUT, SUBMISSION_COPY)
print(f"Created: {OUT}")
print(f"Created: {SUBMISSION_COPY}")
