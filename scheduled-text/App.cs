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
    internal static void Character(char c)
    {
        Input a = new Input(), b = new Input(); a.Type = b.Type = 1;
        if (c == '\n') { a.Data.Keyboard.Key = b.Data.Keyboard.Key = 13; }
        else { a.Data.Keyboard.Scan = b.Data.Keyboard.Scan = c; a.Data.Keyboard.Flags = 4; b.Data.Keyboard.Flags = 4; }
        b.Data.Keyboard.Flags |= 2; Send(a, b);
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
    readonly Button pick = new Button(), start = new Button(), stop = new Button();
    readonly Timer timer = new Timer();
    readonly Schedule schedule = new Schedule();
    Desktop.POINT position, clientPosition, previousCursor;
    IntPtr previousWindow;
    bool borrowedDesktop;
    readonly System.Collections.Generic.List<PositionPicker> pickers = new System.Collections.Generic.List<PositionPicker>();
    IntPtr target;
    bool active, escRegistered, f8Registered;
    long inputDue, lastTick;
    int stage, offset;
    string pending;
    bool appendEnter;

    internal MainWindow()
    {
        Text = "예약 문구 입력";
        Font = new Font("Malgun Gothic", 10); ClientSize = new Size(640, 760);
        AutoScaleMode = AutoScaleMode.Dpi; MinimumSize = new Size(620, 730);
        var layout = new TableLayoutPanel { Dock = DockStyle.Fill, Padding = new Padding(18), ColumnCount = 1, RowCount = 13 };
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
        for (int i = 0; i < 13; i++) layout.RowStyles.Add(new RowStyle(i == 6 ? SizeType.Percent : SizeType.AutoSize, i == 6 ? 100 : 0));
        enter.Text = "입력 후 Enter 한 번 누르기"; enter.AutoSize = true; enter.Checked = true; layout.Controls.Add(enter);
        returnToWork.Text = "입력 후 작업하던 창과 마우스 위치로 돌아가기"; returnToWork.AutoSize = true; returnToWork.Checked = true; layout.Controls.Add(returnToWork);
        layout.Controls.Add(Caption("여러 줄 문구의 줄바꿈도 Enter로 입력됩니다."));
        start.Text = "예약 시작"; start.Size = new Size(260, 40);
        stop.Text = "취소 (Esc)"; stop.Size = new Size(260, 40); stop.Enabled = false;
        layout.Controls.Add(Row(start, stop));
        status.AutoSize = true; status.MinimumSize = new Size(0, 95); status.Text = "대기 중 · 위치와 문구를 설정하세요."; layout.Controls.Add(status);
        layout.Controls.Add(Caption("대기 중 다른 작업 가능 · 입력할 때는 대상 창을 잠깐 활성화합니다."));
        pick.Click += delegate { SelectPosition(); };
        start.Click += delegate { StartSchedule(); }; stop.Click += delegate { Finish("취소했습니다. 이미 입력된 내용은 유지됩니다."); };
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
        pick.Enabled = phrase.Enabled = repeat.Enabled = enter.Enabled = returnToWork.Enabled = start.Enabled = !active;
        stop.Enabled = active;
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
    void StartSchedule() {
        try {
            if (!Desktop.IsWindow(target)) throw new InvalidOperationException("대상 입력칸 위에서 F8로 위치를 지정하세요.");
            if (phrase.Text.Length == 0) throw new InvalidOperationException("입력할 문구를 적어 주세요.");
            long interval = ((long)minutes.Value * 60 + (long)seconds.Value) * 1000;
            long delay;
            if (repeat.Checked || mode.SelectedIndex == 0) {
                if (interval == 0) throw new InvalidOperationException("대기 / 반복 간격을 1초 이상으로 설정하세요.");
                delay = repeat.Checked ? 60000 : interval;
            } else {
                DateTime local = date.Value.Date + time.Value.TimeOfDay;
                delay = (long)(local - DateTime.Now).TotalMilliseconds;
                if (delay <= 0) throw new InvalidOperationException("현재보다 나중인 날짜·시간을 지정하세요.");
                interval = Math.Max(1, delay);
            }
            escRegistered = Desktop.RegisterHotKey(Handle, 2, 0x4000, 0x1B);
            if (!escRegistered) throw new InvalidOperationException("Esc 취소 단축키 등록 실패. 다른 예약 프로그램을 종료해 주세요.");
            borrowedDesktop = false; pending = phrase.Text.Replace("\r\n", "\n").Replace('\r', '\n');
            appendEnter = enter.Checked; schedule.Start(Desktop.Now, delay, interval, repeat.Checked);
            lastTick = Desktop.Now; stage = 0; offset = 0; active = true; Configure(); Progress();
        } catch (Exception ex) { Finish(ex.Message); }
    }
    void PrepareTarget() {
        if (!Desktop.IsWindow(target)) throw new InvalidOperationException("저장한 대상 창이 닫혔습니다. 위치를 다시 지정하세요.");
        previousWindow = Desktop.GetForegroundWindow();
        if (!Desktop.GetCursorPos(out previousCursor)) throw new InvalidOperationException("현재 마우스 위치를 읽지 못했습니다.");
        borrowedDesktop = true;
        if (Desktop.IsIconic(target)) Desktop.ShowWindowAsync(target, 9);
        Desktop.SetForegroundWindow(target);
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
            if (now - lastTick > 2000) throw new InvalidOperationException("절전 또는 실행 지연을 감지하여 중단했습니다.");
            lastTick = now;
            if (stage == 0 && schedule.Ready(now)) {
                if (schedule.Late(now)) throw new InvalidOperationException("예약을 2초 이상 놓쳐 중단했습니다.");
                PrepareTarget(); stage = 1; inputDue = now + 250;
            } else if (stage == 1 && now >= inputDue) {
                position = clientPosition;
                if (!Desktop.ClientToScreen(target, ref position)) throw new InvalidOperationException("저장한 위치를 찾지 못했습니다.");
                CheckTarget(true); Desktop.Click(position); stage = 2; inputDue = now + 300;
            } else if (stage == 2 && now >= inputDue) {
                CheckTarget(true); stage = 3;
            } else if (stage == 3) {
                CheckTarget(true);
                // Small batches leave the UI and Esc cancellation responsive.
                int end = Math.Min(pending.Length, offset + 32);
                if (end < pending.Length && end > offset && Char.IsHighSurrogate(pending[end-1])) end++;
                while (offset < end) Desktop.Character(pending[offset++]);
                if (offset == pending.Length) {
                    if (appendEnter) Desktop.Character('\n');
                    ReturnDesktop();
                    if (!schedule.Complete(Desktop.Now)) { Finish("입력 신호 전송 완료. 대상 화면에서 결과를 확인하세요."); return; }
                    stage = 0; offset = 0;
                }
            }
            Progress();
        } catch (Exception ex) { Finish(ex.Message); }
    }
    static string Duration(long milliseconds) { long s = Math.Max(0, milliseconds) / 1000; return String.Format("{0}분 {1:00}초", s / 60, s % 60); }
    void Progress() {
        long now = Desktop.Now;
        status.Text = "예약 시작됨 · " + (stage == 0 ? "대기 중" : "입력 중") + " · Esc로 취소\r\n경과 " + Duration(now - schedule.Started)
            + " / 다음 입력까지 " + Duration(Math.Max(0, schedule.Due - now + 999))
            + "\r\n" + (schedule.Repeating ? "반복 실행" : "한 번 실행") + " · 입력 신호 전송 " + schedule.Completed + "회";
    }
    void Finish(string message) {
        bool wasActive = active; ReturnDesktop(); active = false; stage = 0;
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
        if (f8Registered) Desktop.UnregisterHotKey(Handle, 1);
        if (escRegistered) Desktop.UnregisterHotKey(Handle, 2);
        foreach (PositionPicker picker in pickers) picker.Dispose();
        base.OnFormClosed(e);
    }
    [STAThread] static void Main() {
        Application.EnableVisualStyles(); Application.SetCompatibleTextRenderingDefault(false); Application.Run(new MainWindow());
    }
}
