using System;
using System.Drawing;
using System.Runtime.InteropServices;
using System.Windows.Forms;

// Native declarations are limited to visible desktop input, hotkeys and uptime.
internal static class Desktop
{
    [StructLayout(LayoutKind.Sequential)] internal struct POINT { public int X, Y; }
    [StructLayout(LayoutKind.Sequential)] internal struct Mouse { public int X, Y; public uint Data, Flags, Time; public UIntPtr Extra; }
    [StructLayout(LayoutKind.Sequential)] internal struct Keyboard { public ushort Key, Scan; public uint Flags, Time; public UIntPtr Extra; }
    [StructLayout(LayoutKind.Explicit)] internal struct Payload {
        [FieldOffset(0)] public Mouse Mouse;
        [FieldOffset(0)] public Keyboard Keyboard;
    }
    [StructLayout(LayoutKind.Sequential)] internal struct Input { public uint Type; public Payload Data; }
    [DllImport("user32.dll", SetLastError=true)] internal static extern bool RegisterHotKey(IntPtr w, int id, uint modifiers, uint key);
    [DllImport("user32.dll")] internal static extern bool UnregisterHotKey(IntPtr w, int id);
    [DllImport("user32.dll")] internal static extern bool GetCursorPos(out POINT p);
    [DllImport("user32.dll")] internal static extern IntPtr WindowFromPoint(POINT p);
    [DllImport("user32.dll")] internal static extern IntPtr GetAncestor(IntPtr w, uint flags);
    [DllImport("user32.dll")] internal static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] internal static extern bool IsWindow(IntPtr w);
    [DllImport("user32.dll")] internal static extern bool IsWindowVisible(IntPtr w);
    [DllImport("user32.dll")] internal static extern bool IsIconic(IntPtr w);
    [DllImport("user32.dll")] internal static extern bool SetForegroundWindow(IntPtr w);
    [DllImport("user32.dll")] internal static extern bool ShowWindowAsync(IntPtr w, int command);
    [DllImport("user32.dll")] internal static extern bool ClientToScreen(IntPtr w, ref POINT p);
    [DllImport("user32.dll")] internal static extern bool ScreenToClient(IntPtr w, ref POINT p);
    [DllImport("user32.dll")] internal static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll", SetLastError=true)] private static extern uint SendInput(uint n, Input[] inputs, int size);
    [DllImport("user32.dll")] internal static extern uint GetClipboardSequenceNumber();
    [DllImport("kernel32.dll")] private static extern ulong GetTickCount64();
    internal static long Now { get { return checked((long)GetTickCount64()); } }
    internal static IntPtr Root(IntPtr h) { return h == IntPtr.Zero ? h : GetAncestor(h, 2); }
    private static void Send(Input down, Input up)
    {
        if (SendInput(2, new Input[] { down, up }, Marshal.SizeOf(typeof(Input))) != 2) {
            // Release a possibly partially injected key or mouse button, then stop.
            SendInput(1, new Input[] { up }, Marshal.SizeOf(typeof(Input)));
            throw new InvalidOperationException("Windows가 입력 전송을 차단했습니다.");
        }
    }
    internal static void Click(POINT p)
    {
        if (!SetCursorPos(p.X, p.Y)) throw new InvalidOperationException("마우스 위치 이동에 실패했습니다.");
        Input a = new Input(), b = new Input();
        a.Data.Mouse.Flags = 2; b.Data.Mouse.Flags = 4; Send(a, b);
    }
    internal static void Paste() {
        Input ctrl = new Input(), v = new Input(), vUp = new Input(), ctrlUp = new Input();
        ctrl.Type = v.Type = vUp.Type = ctrlUp.Type = 1;
        ctrl.Data.Keyboard.Key = ctrlUp.Data.Keyboard.Key = 0x11;
        v.Data.Keyboard.Key = vUp.Data.Keyboard.Key = 0x56;
        vUp.Data.Keyboard.Flags = ctrlUp.Data.Keyboard.Flags = 2;
        Input[] inputs = new Input[] { ctrl, v, vUp, ctrlUp };
        if (SendInput(4, inputs, Marshal.SizeOf(typeof(Input))) != 4) {
            SendInput(2, new Input[] { vUp, ctrlUp }, Marshal.SizeOf(typeof(Input)));
            throw new InvalidOperationException("붙여넣기 키 전송에 실패했습니다.");
        }
    }
    internal static void Character(char c)
    {
        Input a = new Input(), b = new Input(); a.Type = b.Type = 1;
        if (c == '\n') { a.Data.Keyboard.Key = b.Data.Keyboard.Key = 13; }
        else { a.Data.Keyboard.Scan = b.Data.Keyboard.Scan = c; a.Data.Keyboard.Flags = 4; b.Data.Keyboard.Flags = 4; }
        b.Data.Keyboard.Flags |= 2; Send(a, b);
    }
}


// Clipboard data is only used for the explicitly requested paste operation.
internal sealed class ClipboardSession {
    DataObject saved;
    bool owned;
    uint sequence;
    internal void Begin(string text) {
        if (owned) throw new InvalidOperationException("이전 붙여넣기가 아직 끝나지 않았습니다.");
        uint before = Desktop.GetClipboardSequenceNumber();
        IDataObject original = Clipboard.GetDataObject();
        DataObject snapshot = null;
        if (original != null) {
            snapshot = new DataObject();
            foreach (string format in original.GetFormats(false)) {
                object value = original.GetData(format, false);
                if (value == null) throw new InvalidOperationException("기존 클립보드를 보관하지 못해 입력을 중단했습니다.");
                var stream = value as System.IO.MemoryStream;
                if (stream != null) value = new System.IO.MemoryStream(stream.ToArray());
                var image = value as Bitmap;
                if (image != null) value = image.Clone();
                snapshot.SetData(format, false, value);
            }
        }
        if (Desktop.GetClipboardSequenceNumber() != before)
            throw new InvalidOperationException("클립보드가 변경되어 입력을 중단했습니다.");
        saved = snapshot;
        Clipboard.SetText(text, TextDataFormat.UnicodeText);
        sequence = Desktop.GetClipboardSequenceNumber(); owned = true;
    }
    internal void Validate() {
        if (!owned || Desktop.GetClipboardSequenceNumber() != sequence)
            throw new InvalidOperationException("붙여넣기 전에 클립보드가 변경되어 중단했습니다.");
    }
    internal void Restore() {
        if (!owned) return;
        bool unchanged = Desktop.GetClipboardSequenceNumber() == sequence;
        owned = false;
        try {
            // Do not overwrite content copied by the user during automated input.
            if (unchanged) {
                if (saved == null) Clipboard.Clear(); else Clipboard.SetDataObject(saved, true);
            }
        } finally { saved = null; }
    }
}

internal sealed class PositionPicker : Form
{
    internal event Action<Desktop.POINT> Chosen;
    internal event Action Cancelled;
    internal PositionPicker(Rectangle bounds) {
        FormBorderStyle = FormBorderStyle.None; StartPosition = FormStartPosition.Manual;
        Bounds = bounds; BackColor = Color.DodgerBlue; Opacity = 0.18;
        TopMost = true; ShowInTaskbar = false; Cursor = Cursors.Cross; KeyPreview = true;
    }
    protected override void OnMouseDown(MouseEventArgs e) {
        base.OnMouseDown(e);
        if (e.Button == MouseButtons.Right) { if (Cancelled != null) Cancelled(); return; }
        if (e.Button != MouseButtons.Left) return;
        Point screen = PointToScreen(e.Location);
        if (Chosen != null) Chosen(new Desktop.POINT { X = screen.X, Y = screen.Y });
    }
    protected override void OnKeyDown(KeyEventArgs e) {
        if (e.KeyCode == Keys.Escape) { e.Handled = true; if (Cancelled != null) Cancelled(); }
        base.OnKeyDown(e);
    }
}

internal sealed class MainWindow : Form
{
    readonly NumericUpDown minutes = new NumericUpDown(), seconds = new NumericUpDown();
    readonly CheckBox repeat = new CheckBox(), enter = new CheckBox(), returnToWork = new CheckBox();
    readonly ComboBox mode = new ComboBox();
    readonly DateTimePicker date = new DateTimePicker(), time = new DateTimePicker();
    readonly TextBox phrase = new TextBox();
    readonly Label location = new Label(), status = new Label();
    readonly Button pick = new Button(), start = new Button(), stop = new Button(), test = new Button(), resume = new Button();
    readonly Timer timer = new Timer();
    readonly Schedule schedule = new Schedule();
    readonly ClipboardSession clipboard = new ClipboardSession();
    Desktop.POINT position, clientPosition, previousCursor;
    IntPtr previousWindow;
    bool borrowedDesktop;
    readonly System.Collections.Generic.List<PositionPicker> pickers = new System.Collections.Generic.List<PositionPicker>();
    IntPtr target;
    bool active, escRegistered, f8Registered;
    long inputDue, lastTick, activationUntil, retryAt;
    string lastProblem = "";
    int failures;
    bool pasteAttempted;
    int stage;
    string pending;
    bool appendEnter;

    internal MainWindow()
    {
        Text = "예약 문구 입력 v2.3 · 예약 유지";
        Font = new Font("Malgun Gothic", 10); ClientSize = new Size(660, 800);
        AutoScaleMode = AutoScaleMode.Dpi; MinimumSize = new Size(640, 770);
        var layout = new TableLayoutPanel { Dock = DockStyle.Fill, Padding = new Padding(18), ColumnCount = 1, RowCount = 14 };
        Controls.Add(layout);
        layout.Controls.Add(new Label { Text = "예약 클릭 + 문구 입력", AutoSize = true, Font = new Font(Font, FontStyle.Bold) });
        mode.DropDownStyle = ComboBoxStyle.DropDownList;
        mode.Items.AddRange(new object[] { "지금부터 대기", "날짜·시간 지정" }); mode.SelectedIndex = 0; mode.Width = 170;
        minutes.Minimum = 0; minutes.Maximum = 10080; minutes.Value = 5; minutes.Width = 75;
        seconds.Minimum = 0; seconds.Maximum = 59; seconds.Width = 60;
        layout.Controls.Add(Row(mode, minutes, Caption("분"), seconds, Caption("초 (대기 / 반복 간격)")));
        date.Format = DateTimePickerFormat.Custom; date.CustomFormat = "yyyy-MM-dd"; date.Width = 170;
        time.Format = DateTimePickerFormat.Custom; time.CustomFormat = "HH:mm:ss"; time.ShowUpDown = true; time.Width = 130;
        layout.Controls.Add(Row(date, time));
        repeat.Text = "1분 후 시작 · 이후 설정한 분·초 간격으로 반복"; repeat.AutoSize = true;
        layout.Controls.Add(repeat);
        pick.Text = "위치 지정 (화면에서 클릭)"; pick.AutoSize = true;
        location.Text = "대상 입력칸 위에서 F8"; location.AutoSize = true;
        layout.Controls.Add(Row(pick, location));
        layout.Controls.Add(Caption("입력할 문구 (최대 16,000자)"));
        phrase.Multiline = true; phrase.AcceptsReturn = true; phrase.ScrollBars = ScrollBars.Vertical;
        phrase.MaxLength = 16000; phrase.Dock = DockStyle.Fill; phrase.MinimumSize = new Size(100, 160);
        layout.Controls.Add(phrase);
        layout.RowStyles.Clear();
        for (int i = 0; i < 14; i++) layout.RowStyles.Add(new RowStyle(i == 6 ? SizeType.Percent : SizeType.AutoSize, i == 6 ? 100 : 0));
        enter.Text = "입력 후 Enter 한 번 누르기"; enter.AutoSize = true; enter.Checked = true; layout.Controls.Add(enter);
        returnToWork.Text = "입력 후 작업하던 창과 마우스 위치로 돌아가기"; returnToWork.AutoSize = true; returnToWork.Checked = true; layout.Controls.Add(returnToWork);
        layout.Controls.Add(Caption("문구 전체를 붙여넣고 1.2초 후 Enter · 클립보드를 잠시 사용합니다."));
        start.Text = "예약 시작"; start.Size = new Size(260, 40);
        stop.Text = "예약 취소"; stop.Size = new Size(260, 40); stop.Enabled = false;
        layout.Controls.Add(Row(start, stop));
        test.Text = "5초 후 시험 입력 (1회)"; test.AutoSize = true; resume.Text = "확인 후 현재 회차 재시도"; resume.AutoSize = true; resume.Enabled = false; layout.Controls.Add(Row(test, resume));
        status.AutoSize = true; status.MinimumSize = new Size(0, 95); status.Text = "대기 중 · 위치와 문구를 설정하세요."; layout.Controls.Add(status);
        layout.Controls.Add(Caption("대기 중 다른 작업 가능 · 입력할 때는 대상 창을 잠깐 활성화합니다."));
        pick.Click += delegate { SelectPosition(); };
        start.Click += delegate { StartSchedule(false); }; test.Click += delegate { StartSchedule(true); };
        resume.Click += delegate {
            if (!active || stage != 9) return;
            if (MessageBox.Show(this, "대상 입력칸과 전송 내역을 확인하세요. 이미 입력된 내용이 있다면 다시 입력될 수 있습니다. 현재 회차를 재시도할까요?", "재시도 확인", MessageBoxButtons.YesNo, MessageBoxIcon.Question) != DialogResult.Yes) return;
            try { clipboard.Restore(); ReturnDesktop(); schedule.Defer(Desktop.Now + 1000); stage = 0; pasteAttempted = false; lastProblem = ""; lastTick = Desktop.Now; Configure(); Progress(); }
            catch (Exception ex) { lastProblem = ex.Message; Progress(); }
        }; stop.Click += delegate { Finish("취소했습니다. 이미 입력된 내용은 유지됩니다."); };
        repeat.CheckedChanged += delegate { if (repeat.Checked) mode.SelectedIndex = 0; Configure(); };
        mode.SelectedIndexChanged += delegate { Configure(); };
        timer.Interval = 20; timer.Tick += delegate { Tick(); }; timer.Start(); Configure();
    }
    static Label Caption(string text) { return new Label { Text = text, AutoSize = true, Margin = new Padding(3, 8, 3, 8) }; }
    static FlowLayoutPanel Row(params Control[] controls) {
        var panel = new FlowLayoutPanel { AutoSize = true, Dock = DockStyle.Fill, WrapContents = true };
        panel.Controls.AddRange(controls); return panel;
    }
    void Configure() {
        mode.Enabled = !active && !repeat.Checked;
        minutes.Enabled = seconds.Enabled = !active && (repeat.Checked || mode.SelectedIndex == 0);
        date.Enabled = time.Enabled = !active && !repeat.Checked && mode.SelectedIndex == 1;
        pick.Enabled = phrase.Enabled = repeat.Enabled = enter.Enabled = returnToWork.Enabled = start.Enabled = test.Enabled = !active;
        start.Text = active ? "예약 실행 중" : "예약 시작";
        stop.Enabled = active; resume.Enabled = active && stage == 9;
    }
    protected override void OnShown(EventArgs e) {
        base.OnShown(e); f8Registered = Desktop.RegisterHotKey(Handle, 1, 0x4000, 0x77);
        if (!f8Registered) status.Text = "F8을 사용할 수 없습니다. 위치 지정 버튼으로 화면에서 클릭하세요.";
    }
    void SelectPosition() {
        if (active || pickers.Count != 0) return;
        status.Text = "브라우저 입력칸을 클릭하세요. Esc로 위치 선택을 취소할 수 있습니다.";
        Hide();
        foreach (Screen screen in Screen.AllScreens) {
            var picker = new PositionPicker(screen.Bounds);
            picker.Chosen += delegate(Desktop.POINT p) { EndSelection(p, true); };
            picker.Cancelled += delegate { EndSelection(new Desktop.POINT(), false); };
            pickers.Add(picker);
        }
        foreach (PositionPicker picker in pickers) picker.Show();
    }
    void EndSelection(Desktop.POINT p, bool save) {
        foreach (PositionPicker picker in pickers) picker.Hide();
        if (save) RememberPosition(p);
        foreach (PositionPicker picker in pickers) picker.Dispose();
        pickers.Clear(); Show(); Activate();
        if (!save) status.Text = "위치 선택을 취소했습니다.";
    }
    void CapturePosition() {
        if (pickers.Count != 0) return;
        Desktop.POINT p;
        if (!Desktop.GetCursorPos(out p)) { status.Text = "마우스 위치를 읽지 못했습니다."; return; }
        RememberPosition(p);
    }
    void RememberPosition(Desktop.POINT p) {
        IntPtr h = Desktop.Root(Desktop.WindowFromPoint(p));
        if (h == IntPtr.Zero || h == Handle) { status.Text = "이 프로그램 바깥의 입력칸을 지정하세요."; return; }
        Desktop.POINT client = p;
        if (!Desktop.ScreenToClient(h, ref client)) { status.Text = "대상 창의 위치를 저장하지 못했습니다."; return; }
        position = p; clientPosition = client; target = h;
        location.Text = String.Format("저장: X {0} / Y {1}", p.X, p.Y);
        status.Text = "대상 창과 위치 저장 완료 · 예약 시작 후 다른 작업을 하셔도 됩니다.";
    }
    void StartSchedule(bool testing) {
        if (active) return;
        try {
            if (!Desktop.IsWindow(target)) throw new InvalidOperationException("대상 입력칸 위에서 F8로 위치를 지정하세요.");
            if (phrase.Text.Length == 0) throw new InvalidOperationException("입력할 문구를 적어 주세요.");
            long interval = ((long)minutes.Value * 60 + (long)seconds.Value) * 1000;
            long delay;
            if (testing) { delay = 5000; interval = 5000; }
            else if (repeat.Checked || mode.SelectedIndex == 0) {
                if (interval == 0) throw new InvalidOperationException("대기 / 반복 간격을 1초 이상으로 설정하세요.");
                delay = repeat.Checked ? 60000 : interval;
            } else {
                DateTime local = date.Value.Date + time.Value.TimeOfDay;
                delay = (long)(local - DateTime.Now).TotalMilliseconds;
                if (delay <= 0) throw new InvalidOperationException("현재보다 나중인 날짜·시간을 지정하세요.");
                interval = Math.Max(1, delay);
            }
            escRegistered = Desktop.RegisterHotKey(Handle, 2, 0x4003, 0x1B);
            // A busy global Esc key must not prevent starting; the cancel button always works.
            borrowedDesktop = false; pending = phrase.Text.Replace("\r\n", "\n").Replace('\r', '\n');
            appendEnter = enter.Checked; schedule.Start(Desktop.Now, delay, interval, !testing && repeat.Checked);
            lastProblem = ""; failures = 0; pasteAttempted = false;
            lastTick = Desktop.Now; stage = 0; active = true;
            Diagnostics.Write("START delay=" + delay + " repeat=" + (!testing && repeat.Checked)); timer.Start(); Configure(); Progress();
        } catch (Exception ex) { Finish("시작 실패: " + ex.Message); MessageBox.Show(this, ex.Message, "예약 시작 실패", MessageBoxButtons.OK, MessageBoxIcon.Warning); }
    }
    void PrepareTarget() {
        if (!Desktop.IsWindow(target)) throw new InvalidOperationException("저장한 대상 창이 닫혔습니다. 위치를 다시 지정하세요.");
        previousWindow = Desktop.GetForegroundWindow();
        if (!Desktop.GetCursorPos(out previousCursor)) throw new InvalidOperationException("현재 마우스 위치를 읽지 못했습니다.");
        borrowedDesktop = true;
        if (Desktop.IsIconic(target)) Desktop.ShowWindowAsync(target, 9);
        Desktop.SetForegroundWindow(target); activationUntil = Desktop.Now + 3000;
    }
    void CheckTarget(bool focused) {
        if (!Desktop.IsWindow(target) || !Desktop.IsWindowVisible(target) || Desktop.IsIconic(target))
            throw new InvalidOperationException("대상 창을 표시하지 못해 중단했습니다.");
        if (focused && Desktop.Root(Desktop.GetForegroundWindow()) != target)
            throw new InvalidOperationException("대상 창을 활성화하지 못했거나 입력 중 다른 창으로 전환되어 중단했습니다.");
        if (Desktop.Root(Desktop.WindowFromPoint(position)) != target)
            throw new InvalidOperationException("저장 위치가 다른 창에 가려져 입력을 중단했습니다.");
    }
    void ReturnDesktop() {
        if (!borrowedDesktop) return;
        borrowedDesktop = false;
        if (!returnToWork.Checked) return;
        // Do not override a user's deliberate window switch during automated input.
        if (Desktop.Root(Desktop.GetForegroundWindow()) != target) return;
        Desktop.POINT current;
        if (Desktop.GetCursorPos(out current) && current.X == position.X && current.Y == position.Y)
            Desktop.SetCursorPos(previousCursor.X, previousCursor.Y);
        if (previousWindow != IntPtr.Zero && previousWindow != target && Desktop.IsWindow(previousWindow))
            Desktop.SetForegroundWindow(previousWindow);
    }
    void Tick() {
        try {
            long now = Desktop.Now;
            if (!active) return;
            long gap = now - lastTick; lastTick = now;
            if (stage == 9) { Progress(); return; }
            if (gap > 30000) { PauseForReview("절전 또는 30초 이상 실행 지연을 감지했습니다. 대상 창을 확인한 뒤 재시도하세요."); return; }
            if (stage == 7) {
                if (now >= retryAt) { stage = 0; pasteAttempted = false; }
                Progress(); return;
            }
            if (stage == 0 && schedule.Ready(now)) {
                // A small scheduling delay must not disarm the reservation.
                PrepareTarget(); stage = 1; inputDue = now + 250;
            } else if (stage == 1 && now >= inputDue) {
                if (Desktop.Root(Desktop.GetForegroundWindow()) != target && now < activationUntil) {
                    Desktop.SetForegroundWindow(target); inputDue = now + 250; Progress(); return;
                }
                position = clientPosition;
                if (!Desktop.ClientToScreen(target, ref position)) throw new InvalidOperationException("저장한 위치를 찾지 못했습니다.");
                CheckTarget(true); Desktop.Click(position); stage = 2; inputDue = now + 300;
            } else if (stage == 2 && now >= inputDue) {
                CheckTarget(true); stage = 3;
            } else if (stage == 3) {
                CheckTarget(true);
                clipboard.Begin(pending);
                stage = 4; inputDue = now + 150;
            } else if (stage == 4 && now >= inputDue) {
                CheckTarget(true); clipboard.Validate(); pasteAttempted = true; Desktop.Paste();
                stage = 5; inputDue = now + 1200;
            } else if (stage == 5 && now >= inputDue) {
                CheckTarget(true);
                if (appendEnter) Desktop.Character('\n');
                stage = 6; inputDue = now + 1000;
            } else if (stage == 6 && now >= inputDue) {
                CheckTarget(true); clipboard.Restore(); ReturnDesktop();
                if (!schedule.Complete(Desktop.Now)) { Finish("붙여넣기 / Enter 신호 전송 완료. 대상 화면에서 결과를 확인하세요."); return; }
                stage = 0; pasteAttempted = false; lastProblem = "";
                Diagnostics.Write("CYCLE_COMPLETE count=" + schedule.Completed);
            }
            Progress();
        } catch (Exception ex) {
            if (pasteAttempted) PauseForReview("붙여넣기 이후 문제가 발생했습니다. 중복 입력 방지를 위해 확인 대기: " + ex.Message);
            else RetryBeforePaste(ex.Message);
        }
    }
    void RetryBeforePaste(string reason) {
        try { clipboard.Restore(); } catch (Exception ex) { PauseForReview("클립보드 복원 확인 필요: " + ex.Message); return; }
        ReturnDesktop(); failures++; lastProblem = reason;
        stage = 7; retryAt = Desktop.Now + 5000; Configure();
        Diagnostics.Write("RETRY_BEFORE_PASTE #" + failures + " " + reason); Progress();
    }
    void PauseForReview(string reason) {
        try { clipboard.Restore(); } catch (Exception) { reason += " / 클립보드 복원 실패"; }
        ReturnDesktop(); lastProblem = reason; stage = 9; Configure();
        Diagnostics.Write("PAUSED " + reason); Progress();
    }
    static string Duration(long milliseconds) { long s = Math.Max(0, milliseconds) / 1000; return String.Format("{0}분 {1:00}초", s / 60, s % 60); }
    void Progress() {
        long now = Desktop.Now;
        string phase = stage == 9 ? "확인 대기 · 예약 유지" : stage == 7 ? "5초 간격 재시도 · 예약 유지" : stage == 0 ? "대기 중" : stage <= 2 ? "입력칸 준비" : stage <= 4 ? "붙여넣기 준비" : stage == 5 ? "붙여넣기 반영 대기" : "Enter 반영 대기";
        status.Text = "예약 시작됨 · " + phase + (escRegistered ? " · Ctrl+Alt+Esc / 취소 버튼으로 중단" : " · 취소 버튼으로 중단") + "\r\n경과 " + Duration(now - schedule.Started)
            + " / 다음 입력까지 " + Duration(Math.Max(0, schedule.Due - now + 999))
            + "\r\n" + (schedule.Repeating ? "반복 실행" : "한 번 실행") + " · 입력 신호 전송 " + schedule.Completed + "회" + (lastProblem.Length > 0 ? "\r\n최근 상태: " + lastProblem : "");
    }
    void Finish(string message) {
        Diagnostics.Write("STOP " + message);
        bool wasActive = active;
        try { clipboard.Restore(); } catch (Exception) { message += "\r\n기존 클립보드 복원에 실패했습니다."; }
        ReturnDesktop(); active = false; stage = 0;
        if (escRegistered) { Desktop.UnregisterHotKey(Handle, 2); escRegistered = false; }
        pending = null; Configure();
        status.Text = message + (wasActive ? "\r\n총 경과 " + Duration(Desktop.Now - schedule.Started) + " · 입력 신호 전송 " + schedule.Completed + "회" : "");
    }
    protected override void WndProc(ref Message m) {
        if (m.Msg == 0x312) {
            if (m.WParam.ToInt32() == 1 && !active) CapturePosition();
            if (m.WParam.ToInt32() == 2) {
                if (active) Finish("취소했습니다. 이미 입력된 내용은 유지됩니다.");
                else if (pickers.Count != 0) EndSelection(new Desktop.POINT(), false);
            }
        }
        base.WndProc(ref m);
    }
    protected override void OnFormClosed(FormClosedEventArgs e) {
        timer.Stop(); timer.Dispose();
        try { clipboard.Restore(); } catch (Exception) { /* Closing must still release hotkeys. */ }
        if (f8Registered) Desktop.UnregisterHotKey(Handle, 1);
        if (escRegistered) Desktop.UnregisterHotKey(Handle, 2);
        foreach (PositionPicker picker in pickers) picker.Dispose();
        base.OnFormClosed(e);
    }
    [STAThread] static void Main(string[] args) {
        Application.EnableVisualStyles(); Application.SetCompatibleTextRenderingDefault(false);
        if (args.Length == 1 && args[0] == "/install") { Installation.Install(); return; }
        bool first;
        using (var mutex = new System.Threading.Mutex(true, "Local\\ScheduledText.Desktop.v2", out first)) {
            if (!first) { MessageBox.Show("예약 문구 입력이 이미 실행 중입니다. 기존 창을 사용하거나 종료 후 다시 실행하세요.", "중복 실행 안내"); return; }
            try { Application.Run(new MainWindow()); } finally { mutex.ReleaseMutex(); }
        }
    }
}
