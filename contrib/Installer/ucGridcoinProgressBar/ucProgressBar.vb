Public Class ucProgressBar

    Private min As Integer = 0
    ' Minimum value for progress range
    Private max As Integer = 100
    ' Maximum value for progress range
    Private val As Integer = 0
    ' Current progress
    Private BarColor As Color = Color.Green
    ' Color of progress meter
    Protected Overrides Sub OnResize(e As EventArgs)
        ' Invalidate the control to get a repaint.
        Me.Invalidate()
    End Sub

    Protected Overrides Sub OnPaint(e As PaintEventArgs)
        Dim g As Graphics = e.Graphics
        Dim brush As New SolidBrush(BarColor)
        Dim percent As Single = CSng(val - min) / CSng(max - min)
        Dim rect As Rectangle = Me.ClientRectangle

        ' Calculate area for drawing the progress.
        rect.Width = CInt(CSng(rect.Width) * percent)

        ' Draw the progress meter.
        g.FillRectangle(brush, rect)

        ' Draw a three-dimensional border around the control.
        Draw3DBorder(g)

        ' Clean up.
        brush.Dispose()
        g.Dispose()
    End Sub

    Public Property Minimum() As Integer
        Get
            Return min
        End Get

        Set(value As Integer)
            ' Prevent a negative value.
            If value < 0 Then
                min = 0
            End If

            ' Make sure that the minimum value is never set higher than the maximum value.
            If value > max Then
                min = value
                min = value
            End If

            ' Ensure value is still in range
            If val < min Then
                val = min
            End If

            ' Invalidate the control to get a repaint.
            Me.Invalidate()
        End Set
    End Property

    Public Property Maximum() As Integer
        Get
            Return max
        End Get

        Set(value As Integer)
            ' Make sure that the maximum value is never set lower than the minimum value.
            If value < min Then
                min = value
            End If

            max = value

            ' Make sure that value is still in range.
            If val > max Then
                val = max
            End If

            ' Invalidate the control to get a repaint.
            Me.Invalidate()
        End Set
    End Property

    Public Property Value() As Integer
        Get
            Return val
        End Get

        Set(value As Integer)
            Dim oldValue As Integer = val

            ' Make sure that the value does not stray outside the valid range.
            If value < min Then
                val = min
            ElseIf value > max Then
                val = max
            Else
                val = value
            End If

            ' Invalidate only the changed area.
            Dim percent As Single

            Dim newValueRect As Rectangle = Me.ClientRectangle
            Dim oldValueRect As Rectangle = Me.ClientRectangle

            ' Use a new value to calculate the rectangle for progress.
            percent = CSng(val - min) / CSng(max - min)
            newValueRect.Width = CInt(CSng(newValueRect.Width) * percent)

            ' Use an old value to calculate the rectangle for progress.
            percent = CSng(oldValue - min) / CSng(max - min)
            oldValueRect.Width = CInt(CSng(oldValueRect.Width) * percent)

            Dim updateRect As New Rectangle()

            ' Find only the part of the screen that must be updated.
            If newValueRect.Width > oldValueRect.Width Then
                updateRect.X = oldValueRect.Size.Width
                updateRect.Width = newValueRect.Width - oldValueRect.Width
            Else
                updateRect.X = newValueRect.Size.Width
                updateRect.Width = oldValueRect.Width - newValueRect.Width
            End If

            updateRect.Height = Me.Height

            ' Invalidate the intersection region only.
            Me.Invalidate(updateRect)
        End Set
    End Property

    Public Property ProgressBarColor() As Color
        Get
            Return BarColor
        End Get

        Set(value As Color)
            BarColor = value

            ' Invalidate the control to get a repaint.
            Me.Invalidate()
        End Set
    End Property

    Private Sub Draw3DBorder(g As Graphics)
        Dim PenWidth As Integer = CInt(Pens.White.Width) + 0

        g.DrawLine(Pens.DarkGray, New Point(Me.ClientRectangle.Left, Me.ClientRectangle.Top), New Point(Me.ClientRectangle.Width - PenWidth, Me.ClientRectangle.Top))
        g.DrawLine(Pens.DarkGray, New Point(Me.ClientRectangle.Left, Me.ClientRectangle.Top), New Point(Me.ClientRectangle.Left, Me.ClientRectangle.Height - PenWidth))
        g.DrawLine(Pens.Yellow, New Point(Me.ClientRectangle.Left, Me.ClientRectangle.Height - PenWidth), New Point(Me.ClientRectangle.Width - PenWidth, Me.ClientRectangle.Height - PenWidth))
        g.DrawLine(Pens.Yellow, New Point(Me.ClientRectangle.Width - PenWidth, Me.ClientRectangle.Top), New Point(Me.ClientRectangle.Width - PenWidth, Me.ClientRectangle.Height - PenWidth))
    End Sub

End Class
