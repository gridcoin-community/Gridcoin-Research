
Option Strict Off

Friend Structure Matrix

	Public rc11 As Single: Public rc12 As Single: Public rc13 As Single: Public rc14 As Single
	Public rc21 As Single: Public rc22 As Single: Public rc23 As Single: Public rc24 As Single
	Public rc31 As Single: Public rc32 As Single: Public rc33 As Single: Public rc34 As Single
	Public rc41 As Single: Public rc42 As Single: Public rc43 As Single: Public rc44 As Single
End Structure

Friend Module modGRCWireFrameMatrix

    Public Const sPI As Single = 3.14159
    Public Const sPIDiv180 As Single = sPI / 180
    Private IdentityMatrix As Matrix

    Public Sub SetIdentity()

        IdentityMatrix = MatrixIdentity()

    End Sub

    Private Function DegToRad(ByVal Degress As Single) As Single

        Return Degress * (sPIDiv180)

    End Function

    Private Function MatrixIdentity() As Matrix

        With MatrixIdentity
            .rc11 = 1 : .rc12 = 0 : .rc13 = 0 : .rc14 = 0
            .rc21 = 0 : .rc22 = 1 : .rc23 = 0 : .rc24 = 0
            .rc31 = 0 : .rc32 = 0 : .rc33 = 1 : .rc34 = 0
            .rc41 = 0 : .rc42 = 0 : .rc43 = 0 : .rc44 = 1
        End With

    End Function

    Public Function MatrixMultiply(ByRef M1 As Matrix, ByRef M2 As Matrix) As Matrix

        Dim M1t As Matrix
        Dim M2t As Matrix

        M1t = M1
        M2t = M2

        MatrixMultiply = IdentityMatrix

        With MatrixMultiply
            .rc11 = (M1t.rc11 * M2t.rc11) + (M1t.rc21 * M2t.rc12) + (M1t.rc31 * M2t.rc13) + (M1t.rc41 * M2t.rc14)
            .rc12 = (M1t.rc12 * M2t.rc11) + (M1t.rc22 * M2t.rc12) + (M1t.rc32 * M2t.rc13) + (M1t.rc42 * M2t.rc14)
            .rc13 = (M1t.rc13 * M2t.rc11) + (M1t.rc23 * M2t.rc12) + (M1t.rc33 * M2t.rc13) + (M1t.rc43 * M2t.rc14)
            .rc14 = (M1t.rc14 * M2t.rc11) + (M1t.rc24 * M2t.rc12) + (M1t.rc34 * M2t.rc13) + (M1t.rc44 * M2t.rc14)

            .rc21 = (M1t.rc11 * M2t.rc21) + (M1t.rc21 * M2t.rc22) + (M1t.rc31 * M2t.rc23) + (M1t.rc41 * M2t.rc24)
            .rc22 = (M1t.rc12 * M2t.rc21) + (M1t.rc22 * M2t.rc22) + (M1t.rc32 * M2t.rc23) + (M1t.rc42 * M2t.rc24)
            .rc23 = (M1t.rc13 * M2t.rc21) + (M1t.rc23 * M2t.rc22) + (M1t.rc33 * M2t.rc23) + (M1t.rc43 * M2t.rc24)
            .rc24 = (M1t.rc14 * M2t.rc21) + (M1t.rc24 * M2t.rc22) + (M1t.rc34 * M2t.rc23) + (M1t.rc44 * M2t.rc24)

            .rc31 = (M1t.rc11 * M2t.rc31) + (M1t.rc21 * M2t.rc32) + (M1t.rc31 * M2t.rc33) + (M1t.rc41 * M2t.rc34)
            .rc32 = (M1t.rc12 * M2t.rc31) + (M1t.rc22 * M2t.rc32) + (M1t.rc32 * M2t.rc33) + (M1t.rc42 * M2t.rc34)
            .rc33 = (M1t.rc13 * M2t.rc31) + (M1t.rc23 * M2t.rc32) + (M1t.rc33 * M2t.rc33) + (M1t.rc43 * M2t.rc34)
            .rc34 = (M1t.rc14 * M2t.rc31) + (M1t.rc24 * M2t.rc32) + (M1t.rc34 * M2t.rc33) + (M1t.rc44 * M2t.rc34)

            .rc41 = (M1t.rc11 * M2t.rc41) + (M1t.rc21 * M2t.rc42) + (M1t.rc31 * M2t.rc43) + (M1t.rc41 * M2t.rc44)
            .rc42 = (M1t.rc12 * M2t.rc41) + (M1t.rc22 * M2t.rc42) + (M1t.rc32 * M2t.rc43) + (M1t.rc42 * M2t.rc44)
            .rc43 = (M1t.rc13 * M2t.rc41) + (M1t.rc23 * M2t.rc42) + (M1t.rc33 * M2t.rc43) + (M1t.rc43 * M2t.rc44)
            .rc44 = (M1t.rc14 * M2t.rc41) + (M1t.rc24 * M2t.rc42) + (M1t.rc34 * M2t.rc43) + (M1t.rc44 * M2t.rc44)
        End With

    End Function

    Public Function MatrixMultVector(ByRef M As Matrix, ByRef V As Vector) As Vector

        MatrixMultVector.X = (M.rc11 * V.X) + (M.rc12 * V.Y) + (M.rc13 * V.Z) + (M.rc14 * V.W)
        MatrixMultVector.Y = (M.rc21 * V.X) + (M.rc22 * V.Y) + (M.rc23 * V.Z) + (M.rc24 * V.W)
        MatrixMultVector.Z = (M.rc31 * V.X) + (M.rc32 * V.Y) + (M.rc33 * V.Z) + (M.rc34 * V.W)
        MatrixMultVector.W = (M.rc41 * V.X) + (M.rc42 * V.Y) + (M.rc43 * V.Z) + (M.rc44 * V.W)

    End Function

    Public Function MatrixScale(ByVal X As Single, ByVal Y As Single, ByVal Z As Single) As Matrix
     
        MatrixScale = IdentityMatrix
        MatrixScale.rc11 = X
        MatrixScale.rc22 = Y
        MatrixScale.rc33 = Z

    End Function

    Public Function MatrixTranslation(ByVal OffsetX As Single, ByVal OffsetY As Single, ByVal OffsetZ As Single) As Matrix

        MatrixTranslation = IdentityMatrix
        MatrixTranslation.rc14 = OffsetX
        MatrixTranslation.rc24 = OffsetY
        MatrixTranslation.rc34 = OffsetZ

    End Function

    Public Function MatrixRotationX(ByVal Angle As Single) As Matrix

        Dim sngCosine As Single
        Dim sngSine As Single

        sngCosine = Math.Round(Math.Cos(DegToRad(Angle)), 3)
        sngSine = Math.Round(Math.Sin(DegToRad(Angle)), 3)
        MatrixRotationX = IdentityMatrix
        MatrixRotationX.rc22 = sngCosine
        MatrixRotationX.rc23 = -sngSine
        MatrixRotationX.rc32 = sngSine
        MatrixRotationX.rc33 = sngCosine

    End Function

    Public Function MatrixRotationY(ByVal Angle As Single) As Matrix

        Dim sngCosine As Single
        Dim sngSine As Single

        sngCosine = Math.Round(Math.Cos(DegToRad(Angle)), 3)
        sngSine = Math.Round(Math.Sin(DegToRad(Angle)), 3)
        MatrixRotationY = IdentityMatrix
        MatrixRotationY.rc11 = sngCosine
        MatrixRotationY.rc31 = -sngSine
        MatrixRotationY.rc13 = sngSine
        MatrixRotationY.rc33 = sngCosine

    End Function

    Public Function MatrixRotationZ(ByVal Angle As Single) As Matrix

        Dim sngCosine As Single
        Dim sngSine As Single

        sngCosine = Math.Round(Math.Cos(DegToRad(Angle)), 3)
        sngSine = Math.Round(Math.Sin(DegToRad(Angle)), 3)

        MatrixRotationZ = IdentityMatrix
        MatrixRotationZ.rc11 = sngCosine
        MatrixRotationZ.rc21 = sngSine
        MatrixRotationZ.rc12 = -sngSine
        MatrixRotationZ.rc22 = sngCosine

    End Function

    Public Function WorldMatrix() As Matrix
        Dim S As Matrix
        Dim Rx As Matrix
        Dim Ry As Matrix
        Dim Rz As Matrix
        Dim T As Matrix

        With ObjPart

            T = MatrixTranslation(.Position.X, .Position.Y, .Position.Z)
            Rx = MatrixRotationX(.Direction.X)
            Ry = MatrixRotationY(.Direction.Y)
            Rz = MatrixRotationZ(.Direction.Z)

            WorldMatrix = IdentityMatrix
            WorldMatrix = MatrixMultiply(WorldMatrix, Rz)
            WorldMatrix = MatrixMultiply(WorldMatrix, Rx)
            WorldMatrix = MatrixMultiply(WorldMatrix, Ry)
            WorldMatrix = MatrixMultiply(WorldMatrix, T)
        End With

    End Function

End Module
