
Option Explicit Off
Option Strict Off

Imports System.Windows.Forms

Friend Module modGRCWireFrameState
    Private Declare Function GetKeyState Lib "user32" (ByVal nVirtKey As Integer) As Short
    Public Sub UpdatePartPos()
        With ObjPart
            '   If frmGRCWireFrameCanvas.chkAnimation.Value = 1 Then
            'Call UpdateAnim()
            ' Else
            Call UpdateKey()
            ' End If
            .Direction.X = CInt(.Direction.X) Mod 360
            .Direction.Y = CInt(.Direction.Y) Mod 360
            .Direction.Z = CInt(.Direction.Z) Mod 360
            If .Scale < 0.001 Then .Scale = 0.001
            If .Scale > 1000 Then .Scale = 1000
        End With
    End Sub

    Public lasteventX As Long
    Public lasteventY As Long
    Public iRendered As Long
    Public Function RndRange(iRange As Long) As Long
        Dim xyz As Long
        xyz = (Rnd(1) * iRange) + 1
        RndRange = xyz
    End Function
    Public Sub SetLight()

        Light = VectorSet(RndRange(50), RndRange(50), RndRange(300))

        Light = VectorNormalize(Light)
    End Sub

    Public WireFrameMethod(5) As Boolean

    Public Sub mnuRender_Click(ByRef Index As Short)

        Try

            Dim i As Short
            RO.RType = Index
            WireFrameMethod(Index) = Not WireFrameMethod(Index)
            RO.Hidden = IIf(RO.RType = RenderType.Dot Or RO.RType = RenderType.Wireframe, False, True)
            For i = 0 To WireFrameMethod.Length - 1
                WireFrameMethod(i) = IIf(i = Index, True, False)
            Next
            RO.RType = RenderType.Dot 'Prevent Freaking People Out


        Catch ex As Exception
            Log("Wireframe error " + Trim(ex.Message))
        End Try

    End Sub

    Public Sub UpdateGRC(Canvas1 As Form)
        Dim R As Single = 4.2
        Dim S As Single
        Dim T As Single
        Dim z As Integer
        z = Rnd(1) * 100
        Dim y As Integer
        Dim x As Integer
        iRendered = iRendered + 1
        If z < 3 Or iRendered = 1 Then
            'Change position
            x = Rnd(1) * 17
            y = Rnd(1) * 100
            If x = lasteventX Then x = Rnd(1) * 17

            lasteventX = x
            lasteventY = y
            Dim iRndWire As Integer
            iRndWire = Rnd(1) * 100
            If iRndWire < 10 Or iRendered = 1 Then
                Dim iWireType As Integer
                iWireType = Rnd(1) * 5
                'Change Wireframe Type
                Call mnuRender_Click(iWireType)

                SetLight()

            End If
        Else
            x = lasteventX
            y = lasteventY
        End If
        With ObjPart
            S = .Scale * 0.02
            T = 3 / .Scale
            If y < 50 Then

                If x = 1 Then .Position.X += T
                If x = 2 Then .Position.X -= T
                If x = 3 Then .Position.Y += T
                If x = 4 Then .Position.Y -= T
                If x = 5 Then .Position.Z += T
                If x = 6 Then .Position.Z -= T
            Else
                If x = 7 Then .Direction.X += R
                If x = 8 Then .Direction.X -= R
                If x = 9 Then .Direction.Y += R
                If x = 10 Then .Direction.Y -= R
                If x = 11 Then .Direction.Z += R
                If x = 12 Then .Direction.Z -= R
                If z < 21 And x = 13 Then .Scale -= S
                If x = 14 Then .Scale += S
            End If
            If .Position.X > 15 Then x = 15 : .Position.X = .Position.X - 0.25
            If .Position.X < -200 Then .Position.X = -200
            If .Position.Y > 100 Then .Position.Y = 100
            If .Position.Y < 0 Then .Position.Y = 0

            If .Scale < 1.5 Then .Scale = 1.5
            If .Scale < 2 Then .Scale = .Scale + 0.25
            If .Scale > 120 Then .Scale = 120 : .Scale = .Scale - 1

        End With
    End Sub
    Private Sub UpdateKey()
        Dim R As Single = 2.2
        Dim S As Single
        Dim T As Single
        With ObjPart
            S = .Scale * 0.02
            T = 5 / .Scale
            If State(vbKeyShift) Then
                If State(vbKeyD) Then .Position.X += T
                If State(vbKeyA) Then .Position.X -= T
                If State(vbKeyW) Then .Position.Y += T
                If State(vbKeyS) Then .Position.Y -= T
                If State(vbKeyE) Then .Position.Z += T
                If State(vbKeyQ) Then .Position.Z -= T
            Else
                If State(vbKeyS) Then .Direction.X += R
                If State(vbKeyW) Then .Direction.X -= R
                If State(vbKeyD) Then .Direction.Y += R
                If State(vbKeyA) Then .Direction.Y -= R
                If State(vbKeyQ) Then .Direction.Z += R
                If State(vbKeyE) Then .Direction.Z -= R
                If State(vbKeyC) Then .Scale -= S
                If State(vbKeyZ) Then .Scale += S
                If State(vbKeyX) Then Call ResetPos()
                ' If State(vbKeyEscape) Then Unload6(frmGRCWireFrameCanvas) : Application.Exit() : End
            End If
        End With
    End Sub

    Private Sub UpdateAnim()
        Dim i As Short
        Dim R(2) As Single
        Dim T(2) As Single
        For i = 0 To 2
            R(i) = 25 '?  Rotation
            '8-31-2014

            T(i) = 500 '? Speed?

        Next

        With ObjPart
            If T(0) <> 0 Then .Position.X += T(0)
            If T(1) <> 0 Then .Position.Y += T(1)
            If T(2) <> 0 Then .Position.Z += T(2)
            If R(0) <> 0 Then .Direction.X += R(0)
            If R(1) <> 0 Then .Direction.Y += R(1)
            If R(2) <> 0 Then .Direction.Z += R(2)
        End With

    End Sub

    Private Function State(ByVal key As Integer) As Boolean

        Dim lngKeyState As Integer = GetKeyState(key)

        Return IIf((lngKeyState And &H8000S), True, False)

    End Function


    Private Sub ResetPos()

        With ObjPart
            .Direction.X = 0 : .Direction.Y = 0 : .Direction.Z = 0
            .Position.X = 0 : .Position.Y = 0 : .Position.Z = 0
            .Scale = 1
        End With

    End Sub

End Module
