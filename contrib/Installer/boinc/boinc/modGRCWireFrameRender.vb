
Option Strict Off

Imports System.Windows.Forms
Imports System.Drawing

Public Enum RenderType
    Dot
    Dothidden
    Wireframe
    Hidden
    Solid
    SolidFrame
End Enum


Friend Module modGRCWireFrameRender
    Public vbNotSrcCopy As Integer = 3342344
    Public vbMaskNotPen As Integer = 3
    Public vbDstInvert = &H550009
    Public vbMergeCopy = &HC000CA
    Public vbSrcErase = &H440328
    Public vbSrcCopy = &HCC0020

    Private Const PS_SOLID As Short = 0
    Private Const PS_DASH As Short = 1
    Private Const PS_DOT As Short = 2
    Private Const PS_DASHDOT As Short = 3
    Private Const PS_DASHDOTDOT As Short = 4
    Private Const PS_NULL As Short = 5
    Private Const PS_INSIDEFRAME As Short = 6

    Friend Structure RenderOption

        Public RType As RenderType
        Public tColor As ColorRGB
        Public Luminance As Short
        Public Hidden As Boolean
        Public Shade As Boolean
        Public LightOrbit As Boolean
        Public Show As Boolean
        Public ShowIndex As Short
    End Structure

    ' UPGRADE_INFO (#0531): You can replace calls to the 'GetDC' 
    'unamanged method with the following .NET member(s): 
    'System.Drawing.Graphics.FromHwnd(Windows.Forms.Control.Handle), System.Drawing.Printing.PrintPageEventArgs.Graphics, System.Windows.Forms.PaintEventArgs.Graphics, System.Drawing.Graphics.GetHdc
    Public Declare Function GetDC Lib "user32" (ByVal hwnd As Integer) As Integer
    Private Declare Function CreateCompatibleBitmap Lib "gdi32" (ByVal hdc As Integer, ByVal nWidth As Integer, ByVal nHeight As Integer) As Integer
    Private Declare Function CreateCompatibleDC Lib "gdi32" (ByVal hdc As Integer) As Integer
    Private Declare Function ReleaseDC Lib "user32" (ByVal hwnd As Integer, ByVal hdc As Integer) As Integer
    Private Declare Function DeleteDC Lib "gdi32" (ByVal hdc As Integer) As Integer
    Private Declare Function BitBlt Lib "gdi32" (ByVal hDestDC As Integer, ByVal X As Integer, ByVal Y As Integer, ByVal nWidth As Integer, ByVal nHeight As Integer, ByVal hSrcDC As Integer, ByVal xSrc As Integer, ByVal ySrc As Integer, ByVal dwRop As Integer) As Integer
    Private Declare Function SetBkColor Lib "gdi32" (ByVal hdc As Integer, ByVal crColor As Integer) As Integer
    ' UPGRADE_INFO (#0531): You can replace calls to the 'CreatePen' unamanged method with the following .NET member(s): System.Drawing.Pen constructor
    Private Declare Function CreatePen Lib "gdi32" (ByVal nPenStyle As Integer, ByVal nWidth As Integer, ByVal crColor As Integer) As Integer
    ' UPGRADE_INFO (#0531): You can replace calls to the 'CreateSolidBrush' unamanged method with the following .NET member(s): System.Drawing.SolidBrush constructor
    Private Declare Function CreateSolidBrush Lib "gdi32" (ByVal crColor As Integer) As Integer
    Private Declare Function SelectObject Lib "gdi32" (ByVal hdc As Integer, ByVal hObject As Integer) As Integer
    Private Declare Function DeleteObject Lib "gdi32" (ByVal hObject As Integer) As Integer
    ' UPGRADE_INFO (#0531): You can replace calls to the 'Polygon' unamanged method with the following .NET member(s): System.Drawing.Graphics.DrawPolygon, System.Drawing.Graphics.FillPolygon
    Private Declare Function Polygon Lib "gdi32" (ByVal hdc As Integer, ByRef lpPoint As POINTAPI, ByVal nCount As Integer) As Integer
    ' UPGRADE_INFO (#0531): You can replace calls to the 'SetPixel' unamanged method with the following .NET member(s): System.Drawing.Bitmap.SetPixel
    Private Declare Function SetPixel Lib "gdi32" (ByVal hdc As Integer, ByVal X As Integer, ByVal Y As Integer, ByVal crColor As Integer) As Integer
    Private Declare Function MoveToEx Lib "gdi32" (ByVal hdc As Integer, ByVal X As Integer, ByVal Y As Integer, ByRef lpPoint As POINTAPI) As Integer
    ' UPGRADE_INFO (#0531): You can replace calls to the 'LineTo' unamanged method with the following .NET member(s): System.Drawing.Graphics.DrawLine
    Private Declare Function LineTo Lib "gdi32" (ByVal hdc As Integer, ByVal X As Integer, ByVal Y As Integer) As Integer

    'Private Declare Function SetPolyFillMode Lib "gdi32" (ByVal hdc As Long, ByVal nPolyFillMode As Long) As Long
    'Private Declare Function Rectangle Lib "gdi32" (ByVal hdc As Long, ByVal X1 As Long, ByVal Y1 As Long, ByVal X2 As Long, ByVal Y2 As Long) As Long

    Public BackBuffer As Integer
    Public BackBitmap As Integer
    Public oldBackBitmap As Integer

    Public RO As RenderOption
    Public HalfWidth As Integer
    Public HalfHeight As Integer

    'Public MyViewWidth As Single
    'Public MyViewHeight As Single
    Public GlobalGraphics As Graphics
    Public GlobalHDC As IntPtr

    Public Sub InitializeDC(canvas As Form)



        GlobalGraphics = canvas.CreateGraphics
        GlobalHDC = GlobalGraphics.GetHdc

        BackBuffer = CreateCompatibleDC(GlobalHDC)


        BackBitmap = CreateCompatibleBitmap(GlobalHDC, canvas.Width, canvas.Height)

        oldBackBitmap = SelectObject(BackBuffer, BackBitmap)


    End Sub

    Public Sub TerminateDC()

        DeleteDC(BackBuffer)
        DeleteObject(BackBitmap)

    End Sub

    Public Sub Render(ByVal Canvas1 As Form)


        'MainRender


        Dim i As Short
        Dim iV As Short
        ' DoEvents6()
        '    SetPolyFillMode BackBuffer, 2
        With ObjPart
            If RO.Hidden Then
                If ShortVisibleFaces() > -1 Then
                    For i = 0 To .FaceV.Length - 1
                        iV = .FaceV(i).iVisible
                        Call Draw(iV)

                    Next
                End If
            Else
                For i = 0 To .NumFaces
                    Call Draw(i)


                Next
            End If
        End With

        ''''' BitBlt(Canvas1.hDC, 0, 0, Canvas1.ScaleWidth, Canvas1.ScaleHeight, BackBuffer, 0, 0, VBRUN.RasterOpConstants.vbNotSrcCopy)
        BitBlt(GlobalHDC, 0, 0, Canvas1.Width, Canvas1.Height, BackBuffer, 0, 0, vbSrcCopy)

        '8-31-2014
        'BitBlt(frmGRCWireFrameCanvas.hDC.Handle, 0, 0, Canvas1.ScaleWidth, Canvas1.ScaleHeight, BackBuffer, 0, 0, VBRUN.RasterOpConstants.vbNotSrcCopy)


        BitBlt(BackBuffer, 0, 0, Canvas1.Width, Canvas1.Height, BackBuffer, 0, 0, vbMaskNotPen)





        'BitBlt(Canvas1.hDC, 0, 0, Canvas1.ScaleWidth, Canvas1.ScaleHeight, BackBuffer, 0, 0, VBRUN.RasterOpConstants.vbSrcCopy)
        '     BitBlt(BackBuffer, 0, 0, Canvas1.ScaleWidth, Canvas1.ScaleHeight, BackBuffer, 0, 0, VBRUN.DrawModeConstants.vbBlackness)



    End Sub


    Private Sub Draw(ByVal i As Short)


        Dim idx As Short
        Dim tmp(2) As POINTAPI
        Dim Result As Integer
        Dim L As Single
        Dim PartColor As ColorRGB
        Dim lngColor As Integer

        Dim BrushSelect As Integer
        Dim PenSelect As Integer

        If RO.Shade Then
            L = VectorDot(ObjPart.NormalT(i), LightT)
            If L < 0 Then L = 0
            PartColor = ColorScale(RO.tColor, L)
        Else
            PartColor = RO.tColor
        End If

        If i = RO.ShowIndex And RO.Show Then PartColor = ColorInvert(PartColor)
        lngColor = ColorRGBToLong(ColorPlus(PartColor, RO.Luminance))

        With ObjPart
            tmp(0) = .ScreenCoord(.Faces(i).A)
            tmp(1) = .ScreenCoord(.Faces(i).B)
            tmp(2) = .ScreenCoord(.Faces(i).C)
        End With

        Select Case RO.RType
            Case RenderType.Wireframe
                PenSelect = SelectObject(BackBuffer, CreatePen(PS_SOLID, 1, lngColor))
                DrawTriangle(BackBuffer, tmp)
            Case RenderType.Hidden
                PenSelect = SelectObject(BackBuffer, CreatePen(PS_SOLID, 1, lngColor))
                BrushSelect = SelectObject(BackBuffer, CreateSolidBrush(lngColor))
                Polygon(BackBuffer, tmp(0), 3)
            Case RenderType.Solid
                PenSelect = SelectObject(BackBuffer, CreatePen(PS_SOLID, 1, lngColor))
                BrushSelect = SelectObject(BackBuffer, CreateSolidBrush(lngColor))
                Polygon(BackBuffer, tmp(0), 3)
            Case RenderType.SolidFrame
                DeleteObject(SelectObject(BackBuffer, CreatePen(PS_SOLID, 1, 0)))
                BrushSelect = SelectObject(BackBuffer, CreateSolidBrush(lngColor))
                Polygon(BackBuffer, tmp(0), 3)
            Case RenderType.Dot, RenderType.Dothidden
                For idx = 0 To 2
                    Call SetPixel(BackBuffer, tmp(idx).X, tmp(idx).Y, lngColor)
                Next
        End Select

        DeleteObject(PenSelect)
        DeleteObject(BrushSelect)

    End Sub
    Private Sub DrawTriangle(ByVal hdc As Integer, ByRef tmp() As POINTAPI)

        Dim p As POINTAPI

        MoveToEx(hdc, tmp(0).X, tmp(0).Y, p)
        LineTo(hdc, tmp(1).X, tmp(1).Y)
        LineTo(hdc, tmp(2).X, tmp(2).Y)
        LineTo(hdc, tmp(0).X, tmp(0).Y)

    End Sub

    Private Function ShortVisibleFaces() As Short

        Dim IsVisible As Boolean
        Dim i As Short
        Dim iV As Short = -1

        With ObjPart
            '.FaceV.Clear(.FaceV, 0, .FaceV.Length)

            '.FaceV = Nothing
            If Not .FaceV Is Nothing Then Array.Clear(.FaceV, 0, .FaceV.Length - 1)

            'Erase6(.FaceV)
            For i = 0 To .NumFaces
                IsVisible = IIf(VectorDot(ObjPart.NormalT(i), Camera) > 0, True, False)
                If IsVisible Then
                    iV += 1
                    ReDim Preserve .FaceV(iV)
                    .FaceV(iV).ZValue = (.VerticesT(.Faces(i).A).Z + .VerticesT(.Faces(i).B).Z + .VerticesT(.Faces(i).C).Z) '/ 3
                    .FaceV(iV).iVisible = i
                End If
            Next
            If iV > -1 Then Call SortFaces(.FaceV, 0, iV)
            ShortVisibleFaces = iV
        End With

    End Function

    Private Sub SortFaces(ByRef zOrder() As Order, ByVal First As Integer, ByVal Last As Integer)

        Dim FirstIdx As Integer
        Dim MidIdx As Integer
        Dim LastIdx As Integer
        Dim MidVal As Single
        Dim TempOrder As Order

        If (First < Last) Then
            With ObjPart
                MidIdx = (First + Last) \ 2
                MidVal = .FaceV(MidIdx).ZValue
                FirstIdx = First
                LastIdx = Last
                Do
                    Do While .FaceV(FirstIdx).ZValue < MidVal
                        FirstIdx += 1
                    Loop
                    Do While .FaceV(LastIdx).ZValue > MidVal
                        LastIdx -= 1
                    Loop
                    If (FirstIdx <= LastIdx) Then
                        TempOrder = .FaceV(LastIdx)
                        zOrder(LastIdx) = .FaceV(FirstIdx)
                        zOrder(FirstIdx) = TempOrder
                        FirstIdx += 1
                        LastIdx -= 1
                    End If
                Loop Until FirstIdx > LastIdx

                If (LastIdx <= MidIdx) Then
                    Call SortFaces(.FaceV, First, LastIdx)
                    Call SortFaces(.FaceV, FirstIdx, Last)
                Else
                    Call SortFaces(.FaceV, FirstIdx, Last)
                    Call SortFaces(.FaceV, First, LastIdx)
                End If
            End With
        End If

    End Sub


End Module
