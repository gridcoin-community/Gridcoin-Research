Imports Finisar.SQLite
Imports System.Windows.Forms
Imports System.Text

Public Class frmSQL
    Private mData As Sql

    Private Sub frmSQL_Activated(ByVal sender As Object, ByVal e As System.EventArgs) Handles Me.Activated
        Call tSync_Tick(Nothing, Nothing)

    End Sub

    Private Sub frmSQL_Load(sender As System.Object, e As System.EventArgs) Handles MyBase.Load

        Try
            mData = New Sql()

        Catch ex As Exception
            Log("Error while creating sql " + ex.Message)

        End Try
        

        'Query available tables
        Dim dr As New DataTable


        lvTables.View = Windows.Forms.View.Details
        Dim h1 As New System.Windows.Forms.ColumnHeader

        Try
            dr = mGRCData.GetDataTable("SELECT * FROM INTERNALTABLES")

        Catch ex As Exception
            Log("Error while select internaltables " + ex.Message)
            Exit Sub

        End Try

        Dim lRC As Long
        Dim grr As GridcoinReader.GridcoinRow
        Dim iRow As Long

        Try

        lvTables.Columns.Clear()
        lvTables.Columns.Add("Table")
        lvTables.Columns.Add("Rows")

        lvTables.Columns(0).Width = (lvTables.Width * 0.59) - 3
        lvTables.Columns(1).Width = (lvTables.Width * 0.41) - 3

        lvTables.FullRowSelect = True
        lvTables.HeaderStyle = Windows.Forms.ColumnHeaderStyle.Nonclickable
            AddHandler lvTables.DrawColumnHeader, AddressOf lvTables_DrawColumnHeader
        AddHandler lvTables.DrawSubItem, AddressOf lvTables_DrawSubItem
       
        Catch ex As Exception
            MsgBox("Error while retrieving data from InternalTables", MsgBoxStyle.Critical)
            Exit Sub

        End Try


        If dr Is Nothing Then
            MsgBox("Error while retrieving data from InternalTables", MsgBoxStyle.Critical)
            Exit Sub

        End If
        Dim y As Integer = 0

        For y = 0 To dr.Rows.Count - 1
            Dim sTable As String = dr.Rows(y)(0)
            Dim lvItem As New System.Windows.Forms.ListViewItem(sTable)
            Dim rc As String = dr.Rows(y - 1)(1)
            lvItem.SubItems.Add(Trim(rc))
            lvItem.BackColor = Drawing.Color.Black
            lvItem.ForeColor = Drawing.Color.Lime
            lvTables.Items.Add(lvItem)
            iRow = iRow + 1
        Next y

        lvTables.BackColor = Drawing.Color.Black
        lvTables.ForeColor = Drawing.Color.Lime

    End Sub

    Private Sub lvTables_DrawColumnHeader(sender As Object, e As DrawListViewColumnHeaderEventArgs)
        e.Graphics.FillRectangle(Drawing.Brushes.Black, e.Bounds)
        e.Graphics.DrawString(e.Header.Text, lvTables.Font, Drawing.Brushes.Lime, e.Bounds)
        e.Graphics.DrawLine(Drawing.Pens.White, e.Bounds.X, e.Bounds.Y + 15, e.Bounds.X + 40, e.Bounds.Y + 15)
    End Sub
    Private Sub lvTables_DrawSubItem(sender As Object, e As DrawListViewSubItemEventArgs)
        e.Graphics.FillRectangle(Drawing.Brushes.Black, e.Bounds)
        e.DrawText()
    End Sub
    Public Function InsertConfirm(dAmt As Double, sFrom As String, sTo As String, sTXID As String) As String
        Dim sInsert As String
        sInsert = "<INSERT><TABLE>Confirm</TABLE><FIELDS>GRCFrom,GRCTo,txid,amount,Confirmed</FIELDS><VALUES>'" + Trim(sFrom) + "','" + Trim(sTo) + "','" + Trim(sTXID) + "','" + Trim(dAmt) + "','0'</VALUES></INSERT>"
        Dim sErr As String
        sErr = mGRCData.Insert(sInsert)
        Return sErr
    End Function
    Public Function UpdateConfirm(sTXID As String, iStatus As Long) As String
        Return mGRCData.UpdateConfirm(sTXID, iStatus)

    End Function
    Public Function TrackConfirm(sTXID As String) As Integer

        Return mGRCData.TrackConfirm(sTXID)

    End Function

    Private Sub btnExec_Click(sender As System.Object, e As System.EventArgs) Handles btnExec.Click

        If mData Is Nothing Then mData = New Sql

        Dim dr As DataTable
        mData.bThrowUIErrors = True
        Try
            dr = mGRCData.GetDataTable(rtbQuery.Text)
        Catch ex As Exception
            MsgBox(ex.Message, vbCritical, "Gridcoin Query Analayzer")
            Exit Sub
        End Try
        dgv.Rows.Clear()
        dgv.Columns.Clear()
        dgv.BackgroundColor = Drawing.Color.Black
        dgv.ForeColor = Drawing.Color.Lime
        Dim sValue As String
        Dim iRow As Long
        If dr.Rows.Count = 0 Then Exit Sub

        Try
          
            For x = 0 To dr.Columns.Count - 1

                Dim dc As New System.Windows.Forms.DataGridViewColumn
                dc.Name = dr.Columns(x).ColumnName
                Dim dgvct As New System.Windows.Forms.DataGridViewTextBoxCell
                dgvct.Style.BackColor = Drawing.Color.Black
                dgvct.Style.ForeColor = Drawing.Color.Lime
                dc.CellTemplate = dgvct
                dgv.Columns.Add(dc)
            Next x
            Dim dgcc As New DataGridViewCellStyle

            dgcc.ForeColor = System.Drawing.Color.SandyBrown
            dgv.ColumnHeadersDefaultCellStyle = dgcc
            For x = 0 To dr.Columns.Count - 1
                dgv.Columns(x).AutoSizeMode = DataGridViewAutoSizeColumnMode.None
            Next

            For y As Integer = 0 To dr.Rows.Count - 1
                dgv.Rows.Add()
                For x = 0 To dr.Columns.Count - 1
                    sValue = ("" & dr.Rows(y)(x)).ToString
                    dgv.Rows(iRow).Cells(x).Value = sValue
                Next x
                iRow = iRow + 1
            Next
            For x = 0 To dr.Columns.Count - 1
                dgv.Columns(x).AutoSizeMode = DataGridViewAutoSizeColumnMode.AllCells
            Next

            Exit Sub
        Catch ex As Exception
            MsgBox(ex.Message, vbCritical, "Gridcoin Query Analayzer")

        End Try

    End Sub
  
    Public Function GetManifestForTable(sTable As String) As String
        Dim sql As String
        sql = "Select min(id) as lmin From " + sTable
        Dim lStart As Long
        Dim lEnd As Long
        lStart = mData.QueryFirstRow(sql, "lmin")
        sql = "Select max(id) as lmax from " + sTable
        lEnd = mData.QueryFirstRow(sql, "lmax")
        Dim sOut As String
        sOut = Trim(sTable) + "," + Trim(lStart) + "," + Trim(lEnd)
        Return sOut

    End Function

    Private Sub rtbQuery_KeyDown(sender As Object, e As System.Windows.Forms.KeyEventArgs) Handles rtbQuery.KeyDown
        If e.KeyCode = Keys.F5 Then
            Call btnExec_Click(Nothing, Nothing)
        End If
    End Sub

    Private Sub btnRefreshLeaderboard_Click(ByVal sender As System.Object, ByVal e As System.EventArgs)
    End Sub

    Private Sub tSync_Tick(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles tSync.Tick
    End Sub
    Private Function SetPb(ByVal pb As ProgressBar, ByVal valMax As Long, ByVal valActual As Long)
        Dim max As Long
        If valMax > max Then max = valMax
        If valActual > max Then max = valActual
        If max < 40000 Then max = 40000
        pb.Maximum = max
        If valActual < 1 Then valActual = 1
        If valActual > max Then valActual = max
        pb.Value = valActual
        pb.Refresh()
        pb.Update()
        Me.Update()
    End Function

    Private Sub rtbQuery_TextChanged(sender As System.Object, e As System.EventArgs) Handles rtbQuery.TextChanged

    End Sub

    Private Sub dgv_CellContentClick(sender As System.Object, e As System.Windows.Forms.DataGridViewCellEventArgs) Handles dgv.CellContentClick

    End Sub
End Class