
Option Strict Off
Imports System.IO

Friend Structure Vertex
    Public A As Short
	Public B As Short
	Public C As Short
End Structure

Friend Structure Order
    Public ZValue As Single
    Public iVisible As Short
End Structure

Friend Structure POINTAPI
    Public X As Integer
	Public Y As Integer
End Structure

Friend Module modGRCWireFrameObject

    Friend Structure DrawLimits
        Public Min As POINTAPI
        Public Max As POINTAPI
    End Structure

    Friend Structure Part
        Public Caption As String
        Public Position As Vector
        Public Direction As Vector
        Public Vertices() As Vector
        Public VerticesT() As Vector
        Public VerticesV() As Vector
        Public ScreenCoord() As POINTAPI
        Public Normal() As Vector
        Public NormalT() As Vector
        Public Faces() As Vertex
        Public NumVertices As Short
        Public NumFaces As Short
        Public FaceV() As Order
        Public Scale As Single
        Public Color As ColorRGB

#Region "Clone method"

        Public Function Clone() As Part
            Dim copy As Part = Me
            copy.Vertices = Me.Vertices.Clone()
            copy.VerticesT = Me.VerticesT.Clone()
            copy.VerticesV = Me.VerticesV.Clone()
            copy.ScreenCoord = Me.ScreenCoord.Clone()
            copy.Normal = Me.Normal.Clone()
            copy.NormalT = Me.NormalT.Clone()
            copy.Faces = Me.Faces.Clone()
            copy.FaceV = Me.FaceV.Clone()
            Return copy
        End Function

#End Region

    End Structure

    Public Camera As Vector
    Public Light As Vector
    Public LightT As Vector
    Public ObjPart As Part

    Public Sub LoadObject(ByVal strFileName As String)

        Dim i As Short
        Dim rgbcol As ColorRGB
        Dim sOut As String
        Dim sr As New StreamReader(strFileName)
        sOut = sr.ReadToEnd
        sr.Close()
        sOut = Replace(sOut, vbCrLf, ",")

        Dim vOut() As String
        vOut = Split(sOut, ",")

        With ObjPart
            

            'StreamReader sr = new StreamReader("TestFile.txt"))
            'FileOpen6(1, strFileName, OpenMode.Input, OpenAccess.Default, OpenShare.Default, -1)

            .Caption = vOut(0)

            .Scale = Val(vOut(1))


            'FileInput6(1, .Caption)
            'FileInput6(1, .Scale)
            .Color.R = Val(vOut(2))


            'F'ileInput6(1, .Color.R)

            .Color.G = Val(vOut(3))


            'FileInput6(1, .Color.G)
            .Color.B = Val(vOut(4))


            ': FileInput6(1, .Color.B)

            .Direction.X = Val(vOut(5))


            'FileInput6(1, .Direction.X)
            .Direction.Y = Val(vOut(6))

            ': FileInput6(1, .Direction.Y)
            .Direction.Z = Val(vOut(7))

            ': FileInput6(1, .Direction.Z)
            .Position.X = Val(vOut(8))
            .Position.Y = Val(vOut(9))
            .Position.Z = Val(vOut(10))

            'FileInput6(1, .Position.X) : FileInput6(1, .Position.Y) : FileInput6(1, .Position.Z)
            .NumVertices = Val(vOut(11))
            .NumFaces = Val(vOut(12))

            'FileInput6(1, .NumVertices) : FileInput6(1, .NumFaces)
            ReDim .Vertices(.NumVertices)
            ReDim .ScreenCoord(.NumVertices)
            ReDim .Faces(.NumFaces)
            ReDim .Normal(.NumFaces)
            Dim iPos As Long
            iPos = 13
            For i = 0 To (.NumVertices)
                .Vertices(i).X = Val(vOut(iPos)) : iPos += 1
                .Vertices(i).Y = Val(vOut(iPos)) : iPos += 1
                .Vertices(i).Z = Val(vOut(iPos)) : iPos += 1

                'FileInput6(1, .Vertices(i).X) : FileInput6(1, .Vertices(i).Y) : FileInput6(1, .Vertices(i).Z)

                .Vertices(i).W = 1
            Next
            For i = 0 To (.NumFaces)
                .Faces(i).A = Val(vOut(iPos)) : iPos += 1
                .Faces(i).B = Val(vOut(iPos)) : iPos += 1
                .Faces(i).C = Val(vOut(iPos)) : iPos += 1
                '                FileInput6(1, .Faces(i).A) : FileInput6(1, .Faces(i).B) : FileInput6(1, .Faces(i).C)
            Next
            .VerticesT = .Vertices.Clone()
            Call CalculateNormal()

            sr.Close()

            'FileClose6(1)
        End With

    End Sub

End Module
