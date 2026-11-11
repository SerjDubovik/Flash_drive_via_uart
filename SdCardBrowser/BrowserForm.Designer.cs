namespace SdCardBrowser;

partial class BrowserForm
{
    private void InitializeComponent()
    {
        layout = new TableLayoutPanel();
        actionsPanel = new Panel();
        portsComboBox = new ComboBox();
        baudComboBox = new ComboBox();
        mainMenu = new MenuStrip();
        fileMenu = new ToolStripMenuItem("Файл");
        viewMenu = new ToolStripMenuItem("Вид");
        paneMenu = new ToolStripMenuItem("Область");
        helpMenu = new ToolStripMenuItem("Справка");
        exitMenuItem = new ToolStripMenuItem("Выход");
        footerPanel = new TableLayoutPanel();
        freeSpaceLabel = new Label();
        scanButton = new Button();
        connectButton = new Button();
        mediumComboBox = new ComboBox();
        refreshButton = new Button();
        createButton = new Button();
        mkdirButton = new Button();
        renameButton = new Button();
        moveButton = new Button();
        deleteButton = new Button();
        uploadButton = new Button();
        downloadButton = new Button();
        openButton = new Button();
        cancelButton = new Button();
        panes = new SplitContainer();
        cardPanel = new TableLayoutPanel();
        cardHeading = new Label();
        locationBar = new FlowLayoutPanel();
        filesListView = new ListView();
        nameColumn = new ColumnHeader();
        typeColumn = new ColumnHeader();
        sizeColumn = new ColumnHeader();
        pcPanel = new TableLayoutPanel();
        pcToolbar = new FlowLayoutPanel();
        pcHeading = new Label();
        drivesComboBox = new ComboBox();
        pcLocationBar = new FlowLayoutPanel();
        pcListView = new ListView();
        pcNameColumn = new ColumnHeader();
        pcTypeColumn = new ColumnHeader();
        pcSizeColumn = new ColumnHeader();
        statusLabel = new Label();
        transferProgress = new ProgressBar();
        layout.SuspendLayout();
        actionsPanel.SuspendLayout();
        ((System.ComponentModel.ISupportInitialize)panes).BeginInit();
        panes.Panel1.SuspendLayout();
        panes.Panel2.SuspendLayout();
        panes.SuspendLayout();
        cardPanel.SuspendLayout();
        pcPanel.SuspendLayout();
        pcToolbar.SuspendLayout();
        SuspendLayout();
        // 
        // layout
        // 
        layout.ColumnCount = 1;
        layout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100F));
        layout.Controls.Add(actionsPanel, 0, 0);
        layout.Controls.Add(panes, 0, 1);
        layout.Controls.Add(statusLabel, 0, 2);
        layout.Controls.Add(footerPanel, 0, 3);
        layout.Dock = DockStyle.Fill;
        layout.Location = new Point(0, 0);
        layout.Name = "layout";
        layout.Padding = new Padding(12);
        layout.RowCount = 4;
        layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 126F));
        layout.RowStyles.Add(new RowStyle(SizeType.Percent, 100F));
        layout.RowStyles.Add(new RowStyle());
        layout.RowStyles.Add(new RowStyle());
        layout.Size = new Size(1197, 717);
        layout.TabIndex = 0;
        // 
        // actionsPanel
        // 
        actionsPanel.Controls.Add(portsComboBox);
        actionsPanel.Controls.Add(scanButton);
        actionsPanel.Controls.Add(connectButton);
        actionsPanel.Controls.Add(mediumComboBox);
        actionsPanel.Controls.Add(baudComboBox);
        actionsPanel.Controls.Add(refreshButton);
        actionsPanel.Controls.Add(createButton);
        actionsPanel.Controls.Add(mkdirButton);
        actionsPanel.Controls.Add(renameButton);
        actionsPanel.Controls.Add(moveButton);
        actionsPanel.Controls.Add(deleteButton);
        actionsPanel.Controls.Add(uploadButton);
        actionsPanel.Controls.Add(downloadButton);
        actionsPanel.Controls.Add(openButton);
        actionsPanel.Controls.Add(cancelButton);
        actionsPanel.Dock = DockStyle.Fill;
        actionsPanel.Location = new Point(15, 15);
        actionsPanel.Name = "actionsPanel";
        actionsPanel.Size = new Size(1167, 120);
        actionsPanel.TabIndex = 0;
        // 
        // portsComboBox
        // 
        portsComboBox.DropDownStyle = ComboBoxStyle.DropDownList;
        portsComboBox.Location = new Point(3, 3);
        portsComboBox.Name = "portsComboBox";
        portsComboBox.Size = new Size(105, 28);
        portsComboBox.TabIndex = 0;
        // 
        // scanButton
        // 
        scanButton.AutoSize = true;
        scanButton.Location = new Point(114, 3);
        scanButton.Name = "scanButton";
        scanButton.Size = new Size(110, 30);
        scanButton.TabIndex = 1;
        scanButton.Text = "Найти порты";
        scanButton.UseVisualStyleBackColor = true;
        // 
        // connectButton
        // 
        connectButton.AutoSize = true;
        connectButton.Location = new Point(230, 3);
        connectButton.Name = "connectButton";
        connectButton.Size = new Size(120, 30);
        connectButton.TabIndex = 2;
        connectButton.Text = "Подключиться";
        connectButton.UseVisualStyleBackColor = true;
        // 
        // mediumComboBox
        //
        mediumComboBox.AccessibleName = "Накопитель на плате";
        mediumComboBox.DropDownStyle = ComboBoxStyle.DropDownList;
        mediumComboBox.Items.AddRange(new object[] { "SD-карта", "W25Q16" });
        mediumComboBox.Location = new Point(356, 3);
        mediumComboBox.Name = "mediumComboBox";
        mediumComboBox.Size = new Size(140, 28);
        mediumComboBox.TabIndex = 3;
        //
        // baudComboBox
        //
        baudComboBox.AccessibleName = "Скорость USART1, бод";
        baudComboBox.DropDownStyle = ComboBoxStyle.DropDownList;
        baudComboBox.Items.AddRange(new object[] { 9600, 19200, 38400, 57600, 115200, 230400, 460800 });
        baudComboBox.Location = new Point(502, 3);
        baudComboBox.Name = "baudComboBox";
        baudComboBox.Size = new Size(95, 28);
        baudComboBox.TabIndex = 4;
        //
        // refreshButton
        // 
        refreshButton.AutoSize = true;
        refreshButton.Location = new Point(3, 45);
        refreshButton.Name = "refreshButton";
        refreshButton.Size = new Size(88, 30);
        refreshButton.TabIndex = 4;
        refreshButton.Text = "Обновить";
        refreshButton.UseVisualStyleBackColor = true;
        // 
        // createButton
        // 
        createButton.AutoSize = true;
        createButton.Location = new Point(97, 45);
        createButton.Name = "createButton";
        createButton.Size = new Size(106, 30);
        createButton.TabIndex = 5;
        createButton.Text = "Новый файл";
        createButton.UseVisualStyleBackColor = true;
        // 
        // mkdirButton
        // 
        mkdirButton.AutoSize = true;
        mkdirButton.Location = new Point(209, 45);
        mkdirButton.Name = "mkdirButton";
        mkdirButton.Size = new Size(108, 30);
        mkdirButton.TabIndex = 6;
        mkdirButton.Text = "Новая папка";
        mkdirButton.UseVisualStyleBackColor = true;
        // 
        // renameButton
        // 
        renameButton.AutoSize = true;
        renameButton.Location = new Point(323, 45);
        renameButton.Name = "renameButton";
        renameButton.Size = new Size(131, 30);
        renameButton.TabIndex = 7;
        renameButton.Text = "Переименовать";
        renameButton.UseVisualStyleBackColor = true;
        // 
        // moveButton
        // 
        moveButton.AutoSize = true;
        moveButton.Location = new Point(460, 45);
        moveButton.Name = "moveButton";
        moveButton.Size = new Size(110, 30);
        moveButton.TabIndex = 8;
        moveButton.Text = "Переместить";
        moveButton.UseVisualStyleBackColor = true;
        // 
        // deleteButton
        // 
        deleteButton.AutoSize = true;
        deleteButton.Location = new Point(576, 45);
        deleteButton.Name = "deleteButton";
        deleteButton.Size = new Size(75, 30);
        deleteButton.TabIndex = 9;
        deleteButton.Text = "Удалить";
        deleteButton.UseVisualStyleBackColor = true;
        // 
        // uploadButton
        // 
        uploadButton.AutoSize = true;
        uploadButton.Location = new Point(3, 87);
        uploadButton.Name = "uploadButton";
        uploadButton.Size = new Size(160, 30);
        uploadButton.TabIndex = 10;
        uploadButton.Text = "Загрузить…";
        uploadButton.UseVisualStyleBackColor = true;
        // 
        // downloadButton
        // 
        downloadButton.AutoSize = true;
        downloadButton.Location = new Point(169, 87);
        downloadButton.Name = "downloadButton";
        downloadButton.Size = new Size(129, 30);
        downloadButton.TabIndex = 11;
        downloadButton.Text = "Скачать на ПК…";
        downloadButton.UseVisualStyleBackColor = true;
        // 
        // openButton
        // 
        openButton.AutoSize = true;
        openButton.Location = new Point(304, 87);
        openButton.Name = "openButton";
        openButton.Size = new Size(77, 30);
        openButton.TabIndex = 12;
        openButton.Text = "Открыть";
        openButton.UseVisualStyleBackColor = true;
        // 
        // cancelButton
        // 
        cancelButton.AutoSize = true;
        cancelButton.Location = new Point(387, 87);
        cancelButton.Name = "cancelButton";
        cancelButton.Size = new Size(156, 30);
        cancelButton.TabIndex = 13;
        cancelButton.Text = "Отменить передачу";
        cancelButton.UseVisualStyleBackColor = true;
        // 
        // panes
        // 
        panes.Dock = DockStyle.Fill;
        panes.Location = new Point(15, 141);
        panes.Name = "panes";
        // 
        // panes.Panel1
        // 
        panes.Panel1.Controls.Add(cardPanel);
        panes.Panel1MinSize = 245;
        // 
        // panes.Panel2
        // 
        panes.Panel2.Controls.Add(pcPanel);
        panes.Panel2MinSize = 245;
        panes.Size = new Size(1167, 521);
        panes.SplitterDistance = 581;
        panes.TabIndex = 1;
        // 
        // cardPanel
        // 
        cardPanel.ColumnCount = 1;
        cardPanel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100F));
        cardPanel.Controls.Add(cardHeading, 0, 0);
        cardPanel.Controls.Add(locationBar, 0, 1);
        cardPanel.Controls.Add(filesListView, 0, 2);
        cardPanel.Dock = DockStyle.Fill;
        cardPanel.Location = new Point(0, 0);
        cardPanel.Name = "cardPanel";
        cardPanel.RowCount = 3;
        cardPanel.RowStyles.Add(new RowStyle(SizeType.Absolute, 34F));
        cardPanel.RowStyles.Add(new RowStyle(SizeType.Absolute, 46F));
        cardPanel.RowStyles.Add(new RowStyle(SizeType.Percent, 100F));
        cardPanel.Size = new Size(581, 521);
        cardPanel.TabIndex = 0;
        // 
        // cardHeading
        // 
        cardHeading.AutoSize = true;
        cardHeading.Location = new Point(3, 6);
        cardHeading.Margin = new Padding(3, 6, 3, 0);
        cardHeading.Name = "cardHeading";
        cardHeading.Size = new Size(121, 20);
        cardHeading.TabIndex = 0;
        cardHeading.Text = "SD-карта STM32";
        // 
        // locationBar
        // 
        locationBar.AccessibleName = "Путь на накопителе";
        locationBar.AutoScroll = true;
        locationBar.Dock = DockStyle.Fill;
        locationBar.Location = new Point(3, 37);
        locationBar.Name = "locationBar";
        locationBar.Size = new Size(575, 40);
        locationBar.TabIndex = 1;
        locationBar.WrapContents = false;
        // 
        // filesListView
        // 
        filesListView.AccessibleName = "Файлы накопителя";
        filesListView.AllowDrop = true;
        filesListView.Columns.AddRange(new ColumnHeader[] { nameColumn, typeColumn, sizeColumn });
        filesListView.Dock = DockStyle.Fill;
        filesListView.FullRowSelect = true;
        filesListView.Location = new Point(3, 83);
        filesListView.Name = "filesListView";
        filesListView.Size = new Size(575, 435);
        filesListView.TabIndex = 2;
        filesListView.UseCompatibleStateImageBehavior = false;
        filesListView.View = View.Details;
        // 
        // nameColumn
        // 
        nameColumn.Text = "Имя";
        nameColumn.Width = 268;
        // 
        // typeColumn
        // 
        typeColumn.Text = "Тип";
        typeColumn.Width = 93;
        // 
        // sizeColumn
        // 
        sizeColumn.Text = "Размер, байт";
        sizeColumn.TextAlign = HorizontalAlignment.Right;
        sizeColumn.Width = 115;
        // 
        // pcPanel
        // 
        pcPanel.ColumnCount = 1;
        pcPanel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100F));
        pcPanel.Controls.Add(pcToolbar, 0, 0);
        pcPanel.Controls.Add(pcLocationBar, 0, 1);
        pcPanel.Controls.Add(pcListView, 0, 2);
        pcPanel.Dock = DockStyle.Fill;
        pcPanel.Location = new Point(0, 0);
        pcPanel.Name = "pcPanel";
        pcPanel.RowCount = 3;
        pcPanel.RowStyles.Add(new RowStyle(SizeType.Absolute, 34F));
        pcPanel.RowStyles.Add(new RowStyle(SizeType.Absolute, 46F));
        pcPanel.RowStyles.Add(new RowStyle(SizeType.Percent, 100F));
        pcPanel.Size = new Size(582, 521);
        pcPanel.TabIndex = 0;
        // 
        // pcToolbar
        // 
        pcToolbar.Controls.Add(pcHeading);
        pcToolbar.Controls.Add(drivesComboBox);
        pcToolbar.Dock = DockStyle.Fill;
        pcToolbar.Location = new Point(0, 0);
        pcToolbar.Margin = new Padding(0);
        pcToolbar.Name = "pcToolbar";
        pcToolbar.Size = new Size(582, 34);
        pcToolbar.TabIndex = 0;
        pcToolbar.WrapContents = false;
        // 
        // pcHeading
        // 
        pcHeading.AutoSize = true;
        pcHeading.Location = new Point(3, 6);
        pcHeading.Margin = new Padding(3, 6, 8, 0);
        pcHeading.Name = "pcHeading";
        pcHeading.Size = new Size(90, 20);
        pcHeading.TabIndex = 0;
        pcHeading.Text = "Компьютер";
        // 
        // drivesComboBox
        // 
        drivesComboBox.AccessibleName = "Диск компьютера";
        drivesComboBox.DropDownStyle = ComboBoxStyle.DropDownList;
        drivesComboBox.Location = new Point(104, 3);
        drivesComboBox.Name = "drivesComboBox";
        drivesComboBox.Size = new Size(84, 28);
        drivesComboBox.TabIndex = 1;
        // 
        // pcLocationBar
        // 
        pcLocationBar.AccessibleName = "Путь на компьютере";
        pcLocationBar.AutoScroll = true;
        pcLocationBar.Dock = DockStyle.Fill;
        pcLocationBar.Location = new Point(3, 37);
        pcLocationBar.Name = "pcLocationBar";
        pcLocationBar.Size = new Size(576, 40);
        pcLocationBar.TabIndex = 1;
        pcLocationBar.WrapContents = false;
        // 
        // pcListView
        // 
        pcListView.AccessibleName = "Файлы компьютера";
        pcListView.AllowDrop = true;
        pcListView.Columns.AddRange(new ColumnHeader[] { pcNameColumn, pcTypeColumn, pcSizeColumn });
        pcListView.Dock = DockStyle.Fill;
        pcListView.FullRowSelect = true;
        pcListView.Location = new Point(3, 83);
        pcListView.Name = "pcListView";
        pcListView.Size = new Size(576, 435);
        pcListView.TabIndex = 2;
        pcListView.UseCompatibleStateImageBehavior = false;
        pcListView.View = View.Details;
        // 
        // pcNameColumn
        // 
        pcNameColumn.Text = "Имя";
        pcNameColumn.Width = 268;
        // 
        // pcTypeColumn
        // 
        pcTypeColumn.Text = "Тип";
        pcTypeColumn.Width = 93;
        // 
        // pcSizeColumn
        // 
        pcSizeColumn.Text = "Размер, байт";
        pcSizeColumn.TextAlign = HorizontalAlignment.Right;
        pcSizeColumn.Width = 115;
        // 
        // statusLabel
        // 
        statusLabel.AutoSize = true;
        statusLabel.Location = new Point(15, 665);
        statusLabel.Name = "statusLabel";
        statusLabel.Size = new Size(203, 20);
        statusLabel.TabIndex = 3;
        statusLabel.Text = "Выберите COM-порт платы.";
        // 
        // footerPanel
        //
        footerPanel.AutoSize = true;
        footerPanel.ColumnCount = 2;
        footerPanel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100F));
        footerPanel.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        footerPanel.RowCount = 1;
        footerPanel.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        footerPanel.Dock = DockStyle.Fill;
        footerPanel.Name = "footerPanel";
        footerPanel.Controls.Add(transferProgress, 0, 0);
        footerPanel.Controls.Add(freeSpaceLabel, 1, 0);
        freeSpaceLabel.AutoSize = true;
        freeSpaceLabel.Anchor = AnchorStyles.Right;
        freeSpaceLabel.Name = "freeSpaceLabel";
        freeSpaceLabel.Text = "Свободно —";
        //
        // transferProgress
        // 
        transferProgress.AccessibleName = "Прогресс передачи";
        transferProgress.Dock = DockStyle.Fill;
        transferProgress.Location = new Point(15, 688);
        transferProgress.Name = "transferProgress";
        transferProgress.Size = new Size(1167, 14);
        transferProgress.TabIndex = 4;
        transferProgress.Visible = false;
        // 
        // BrowserForm
        // 
        AutoScaleDimensions = new SizeF(120F, 120F);
        AutoScaleMode = AutoScaleMode.Dpi;
        ClientSize = new Size(1197, 717);
        fileMenu.DropDownItems.Add(exitMenuItem);
        mainMenu.Items.AddRange(new ToolStripItem[] { fileMenu, viewMenu, paneMenu, helpMenu });
        mainMenu.Dock = DockStyle.Top;
        mainMenu.Name = "mainMenu";
        MainMenuStrip = mainMenu;
        Controls.Add(layout);
        Controls.Add(mainMenu);
        MinimumSize = new Size(820, 420);
        Name = "BrowserForm";
        StartPosition = FormStartPosition.CenterScreen;
        Text = "Файлы на SD-карте STM32";
        layout.ResumeLayout(false);
        layout.PerformLayout();
        actionsPanel.ResumeLayout(false);
        actionsPanel.PerformLayout();
        panes.Panel1.ResumeLayout(false);
        panes.Panel2.ResumeLayout(false);
        ((System.ComponentModel.ISupportInitialize)panes).EndInit();
        panes.ResumeLayout(false);
        cardPanel.ResumeLayout(false);
        cardPanel.PerformLayout();
        pcPanel.ResumeLayout(false);
        pcToolbar.ResumeLayout(false);
        pcToolbar.PerformLayout();
        ResumeLayout(false);
    }

    private TableLayoutPanel layout;
    private Panel actionsPanel;
    private ComboBox portsComboBox;
    private ComboBox baudComboBox;
    private MenuStrip mainMenu;
    private ToolStripMenuItem fileMenu;
    private ToolStripMenuItem viewMenu;
    private ToolStripMenuItem paneMenu;
    private ToolStripMenuItem helpMenu;
    private ToolStripMenuItem exitMenuItem;
    private TableLayoutPanel footerPanel;
    private Label freeSpaceLabel;
    private Button scanButton;
    private Button connectButton;
    private ComboBox mediumComboBox;
    private Button refreshButton;
    private Button createButton;
    private Button mkdirButton;
    private Button renameButton;
    private Button moveButton;
    private Button deleteButton;
    private Button uploadButton;
    private Button downloadButton;
    private Button openButton;
    private Button cancelButton;
    private SplitContainer panes;
    private TableLayoutPanel cardPanel;
    private TableLayoutPanel pcPanel;
    private Label cardHeading;
    private FlowLayoutPanel pcToolbar;
    private Label pcHeading;
    private ComboBox drivesComboBox;
    private FlowLayoutPanel pcLocationBar;
    private ListView pcListView;
    private ColumnHeader pcNameColumn;
    private ColumnHeader pcTypeColumn;
    private ColumnHeader pcSizeColumn;
    private FlowLayoutPanel locationBar;
    private ProgressBar transferProgress;
    private Label statusLabel;
    private ListView filesListView;
    private ColumnHeader nameColumn;
    private ColumnHeader typeColumn;
    private ColumnHeader sizeColumn;
}
