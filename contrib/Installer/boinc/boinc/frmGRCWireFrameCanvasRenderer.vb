

Option Strict Off

Imports System.Reflection
Imports System.Runtime.InteropServices
Imports System.Drawing
Imports System.Windows.Forms

Public Class frmGRCWireFrameCanvas
    Public Elapsed As Integer
    Public bRendering As Boolean = False

    Private FPS As Short

    Private Sub chkShow_Click()
        RO.Show = False
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
        Me.Left = Screen.PrimaryScreen.WorkingArea.Width / 2 - (Me.Width / 2)

        Me.Top = Screen.PrimaryScreen.WorkingArea.Height / 2 - (Me.Height / 2)


        HalfWidth = Me.Width / 2

        HalfHeight = Me.Height / 2



        Call TerminateDC()
        Call InitializeDC(Me)



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
        Me.Visible = False

        'Call InitializeDC(Me)



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


        On Error GoTo HeinousExit

        Log("Loading Wireframe")

        Call LoadObject(GetGRCAppDir() + "\headwireframe.dll")


        ''''''''''''''''''''''''''''''''''''''''''''''''''''
        Dim PosMatrix As Matrix
        Dim i As Integer
        Call mnuRender_Click(1)
        Log("Setting opacity")
        'Me.BackColor = Color.Black
        Me.Opacity = 0.99
        SetStyle(ControlStyles.SupportsTransparentBackColor, True)
        Me.AllowTransparency = True
        Me.BackColor = Color.Transparent
        Me.TransparencyKey = Color.Black

        With ObjPart


            Call UpdateROColor(ColorSet(10, 235, 10))
            cmdApply_Click()
            RO.Luminance = 128
            RO.Hidden = False
            RO.Shade = True
            bRendering = True
            Log("Rendering")
            Me.Visible = True

            Do
                Call UpdatePartPos()
                If bRendering = False Then Exit Sub

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
                Application.DoEvents()

                FPS += 1

                Call modGRCWireFrameState.UpdateGRC(Me)

            Loop
        End With
        Exit Sub

heinousExit:
        Log("Error" + Err.Description)
        Me.EndWireFrame()

    End Sub


    Private Sub picCanvas_MouseMove(ByRef Button As System.Int16, ByRef Shift As System.Int16, ByRef X As System.Single, ByRef Y As System.Single)

    End Sub
    Private Sub picCanvas_MouseUp(ByRef Button As System.Int16, ByRef Shift As System.Int16, ByRef X As System.Single, ByRef Y As System.Single)

    End Sub

    Protected Overrides Sub Finalize()

        Call TerminateDC()

        MyBase.Finalize()
    End Sub
    Public Sub EndWireFrame()
        bRendering = False

        Me.Finalize()
        Me.Hide()


    End Sub

    Private Sub TimerElapsed_Tick(sender As System.Object, e As System.EventArgs) Handles TimerElapsed.Tick
        Elapsed += 1
        If Elapsed > 60 Then
            bRendering = False

            Me.Finalize()
            Me.Hide()

        End If
    End Sub

    Public Sub New()

        ' This call is required by the designer.
        InitializeComponent()

        ' Add any initialization after the InitializeComponent() call.

    End Sub
End Class










