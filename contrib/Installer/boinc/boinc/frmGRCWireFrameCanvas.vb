
Option Strict Off

Imports System.Reflection
Imports System.Runtime.InteropServices

Friend Class frmGRCWireFrameCanvas

    Private FPS As Short

    Private Sub chkShow_Click()
        RO.Show = False

        '  scrFaces.Enabled = .Value
        ' RO.Show = .Value

    End Sub

    Private Sub cmdApply_Click()

        Light = VectorSet(0, 0, 300)
        Light = VectorNormalize(Light)

    End Sub

    Private Sub frmCanvas_Activated(sender As Object, e As System.EventArgs) Handles Me.Activated
        Randomize()

        LoadGRCWireFrameRenderer()


    End Sub

    Private Sub Form_Resize() Handles MyBase.Resize


        If Me.Width > 5 Then
            '
            ' picCanvas.Move(0, 0, Me.ScaleWidth - picCarry.Width - 5, Me.ScaleHeight)
            ' picCarry.Move(picCanvas.ScaleWidth + 10, 5)

            HalfWidth = Me.Width / 2

            HalfHeight = Me.Height / 2


            ' Debug.Print(picCanvas.ScaleWidth, Me.Width)


            Call TerminateDC()
            Call InitializeDC()



        End If

    End Sub


    Public WireFrameMethod(5) As Boolean



    Public Sub mnuRender_Click(ByRef Index As Short)

        Dim i As Short

        RO.RType = Index
        WireFrameMethod(Index) = Not WireFrameMethod(Index)

        RO.Hidden = IIf(RO.RType = RenderType.Dot Or RO.RType = RenderType.Wireframe, False, True)
        For i = 0 To WireFrameMethod.Length - 1
            WireFrameMethod(i) = IIf(i = Index, True, False)
        Next

    End Sub




    Private Sub scrFaces_Change()

        RO.ShowIndex = 5

        Stop

        '  lblFaces.Caption = scrFaces.Value
        '  RO.ShowIndex = scrFaces.Value

    End Sub

    Private Sub scrFaces_Scroll()
        RO.ShowIndex = 5


    End Sub

    Private Sub scrLuminance_Change()

        RO.Luminance = 100


    End Sub

    Dim Grp As Graphics
    Public hDC As HandleRef
    Public srcBmp As System.Drawing.Image
    Public MemoryGrpObj As System.Drawing.Graphics
    Public MemoryHdc As IntPtr
    Dim hDCPanel As HandleRef
    Dim GrpPanel As Graphics

    Public MyBitbliter As New BitBliter()

    Private Sub Form_HandleCreated(ByVal sender As Object, ByVal e As System.EventArgs) Handles Me.HandleCreated


        hDC = New HandleRef(Me, Me.CreateGraphics.GetHdc)

        'hDCPanel = New HandleRef(GrpPanel, GrpPanel.GetHdc)

        '  Grp.ReleaseHdc(hDC.Handle)
        ' We Create a Bitmap object at the desired size,
        'srcBmp = New Bitmap(Me.Width, Me.Height, Grp)

        ' Creating Graphics Objects
        '  MemoryGrpObj = Graphics.FromImage(srcBmp) 'Create a Graphics object

        ' Creating a Device Context
        ' MemoryHdc = MemoryGrpObj.GetHdc 'Get the Device Context


    End Sub

    Private Sub Form_FormClosing(ByVal sender As Object, ByVal e As System.Windows.Forms.FormClosingEventArgs) Handles Me.FormClosing
        DirectCast(hDC.Wrapper, Graphics).ReleaseHdc()
        Grp.Dispose()
    End Sub

    Private Sub Form_Load() Handles MyBase.Load

        Call InitializeDC()


        Call SetIdentity()
        Call chkShow_Click()
        Call mnuRender_Click(4)
        'Me.BorderStyle = VBRUN.FormBorderStyleConstants.vbBSNone
        Me.FormBorderStyle = Windows.Forms.FormBorderStyle.None

        RO.Shade = True

        ' scrFaces.Enabled = False
        Camera.X = 0
        Camera.Y = 0
        Camera.Z = 1
    End Sub

    Private Sub cmdRandom_Click()
        Call UpdateROColor(ColorRandom())
    End Sub

    Private Sub UpdateROColor(ByRef C As ColorRGB)
        RO.tColor = C
    
    End Sub
    Public Function GetGRCAppDir() As String
        Try
            Dim fi As New System.IO.FileInfo(Assembly.GetExecutingAssembly().Location)
            Return fi.DirectoryName
        Catch ex As Exception
        End Try
    End Function
    Private Sub LoadGRCWireFrameRenderer()


        On Error Resume Next
        Call LoadObject(GetGRCAppDir() + "\Head.prt")
        ' Call LoadObject("C:\egl25net\EGL25_NET\Object\duck.prt")

        ''''''''''''''''''''''''''''''''''''''''''''''''''''
        Dim PosMatrix As Matrix
        Dim i As Integer
        Call mnuRender_Click(1)

        Me.BackColor = Color.Black


        With ObjPart

      
            Call UpdateROColor(ColorSet(235, 0, 235))
            cmdApply_Click()
            RO.Luminance = 100
            RO.Hidden = False
            RO.Shade = True

            Do
                Call UpdatePartPos()

                PosMatrix = WorldMatrix()

                For i = 0 To .NumVertices
                    .VerticesT(i) = MatrixMultVector(PosMatrix, .Vertices(i))
                    .VerticesT(i) = VectorScale(.VerticesT(i), .Scale)
                    .ScreenCoord(i).X = .VerticesT(i).X + HalfWidth
                    .ScreenCoord(i).Y = -.VerticesT(i).Y + HalfHeight
                Next

                For i = 0 To .NumFaces
                    .NormalT(i) = MatrixMultVector(PosMatrix, .Normal(i))
                Next

                LightT = Light
              
                Call Render(Me)

                FPS += 1

                Call modGRCWireFrameState.UpdateGRC()

            Loop
        End With

    End Sub
   

    Private Sub picCanvas_MouseMove(ByRef Button As System.Int16, ByRef Shift As System.Int16, ByRef X As System.Single, ByRef Y As System.Single)

    End Sub
    Private Sub picCanvas_MouseUp(ByRef Button As System.Int16, ByRef Shift As System.Int16, ByRef X As System.Single, ByRef Y As System.Single)

    End Sub

    Protected Overrides Sub Finalize()

        Call TerminateDC()

        MyBase.Finalize()
    End Sub
End Class
