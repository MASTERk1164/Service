using System;

// Pure scheduling logic: no OS, file, keyboard or network access.
public sealed class Schedule
{
    public long Due { get; private set; }
    public long Started { get; private set; }
    public long Interval { get; private set; }
    public int Completed { get; private set; }
    public bool Repeating { get; private set; }
    public void Start(long now, long delay, long interval, bool repeat)
    {
        if (delay <= 0 || interval <= 0) throw new ArgumentOutOfRangeException();
        Started = now; Due = checked(now + delay); Interval = interval;
        Repeating = repeat; Completed = 0;
    }
    public void Defer(long due) { Due = due; }
    public bool Ready(long now) { return now >= Due; }
    public bool Late(long now) { return now > Due && now - Due > 2000; }
    public bool Complete(long now)
    {
        Completed++;
        if (!Repeating) return false;
        long next = checked(Due + Interval);
        if (next <= now) next = checked(next + checked(((now - next) / Interval + 1) * Interval));
        Due = next;
        return true;
    }
}
