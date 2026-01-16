// frontend/src/pages/DatabaseExplorer/DataTable.tsx
import React, { useState, useEffect, useCallback, useMemo } from 'react';
import { Table, Button, Space, Typography, Tag, Modal, Form, Input, InputNumber, Select, Tooltip, message } from 'antd';
import {
    ReloadOutlined,
    EditOutlined,
    DeleteOutlined,
    KeyOutlined,
    ExclamationCircleOutlined,
    SaveOutlined,
    CloseOutlined
} from '@ant-design/icons';
import { DatabaseApiService, ColumnInfo, TableDataResponse } from '../../api/services/databaseApi';

const { Title, Text } = Typography;
const { confirm } = Modal;

interface DataTableProps {
    tableName: string;
}

const DataTable: React.FC<DataTableProps> = ({ tableName }) => {
    const [schema, setSchema] = useState<ColumnInfo[]>([]);
    const [data, setData] = useState<any[]>([]);
    const [pagination, setPagination] = useState<TableDataResponse['pagination']>({
        total: 0,
        page: 1,
        limit: 20,
        totalPages: 1
    });
    const [loading, setLoading] = useState(false);
    const [error, setError] = useState<string | null>(null);

    // Editing State
    const [editingRow, setEditingRow] = useState<any | null>(null);
    const [selectedRowId, setSelectedRowId] = useState<string | number | null>(null);
    const [form] = Form.useForm();

    // Sort
    const [sortField, setSortField] = useState<string | undefined>();
    const [sortOrder, setSortOrder] = useState<'ASC' | 'DESC'>('ASC');

    useEffect(() => {
        if (tableName) {
            loadSchemaAndData();
            setSortField(undefined);
            setPagination(prev => ({ ...prev, page: 1 }));
        }
    }, [tableName]);



    // Memoized Handlers
    const loadData = useCallback(async (page: number, currentSortField?: string, currentSortOrder?: 'ASC' | 'DESC') => {
        setLoading(true);
        try {
            const dataRes = await DatabaseApiService.getTableData(tableName, {
                page,
                limit: pagination.limit,
                sort: currentSortField || sortField,
                order: currentSortOrder || sortOrder
            });

            if (dataRes.success && dataRes.data) {
                const itemsWithKey = dataRes.data.items.map((item, index) => ({
                    ...item,
                    _row_idx: index
                }));
                setData(itemsWithKey);
                setPagination(dataRes.data.pagination);
            }
        } catch (err: any) {
            console.error(err);
        } finally {
            setLoading(false);
        }
    }, [tableName, pagination.limit, sortField, sortOrder]);

    const loadSchemaAndData = useCallback(async () => {
        setLoading(true);
        setError(null);
        try {
            const schemaRes = await DatabaseApiService.getTableSchema(tableName);
            if (!schemaRes.success || !schemaRes.data) throw new Error('Failed to load schema');

            const loadedSchema = Array.isArray(schemaRes.data.schema) ? schemaRes.data.schema : [];
            setSchema(loadedSchema);

            await loadData(1);
        } catch (err: any) {
            setError(err.message || 'Failed to load table data');
            setSchema([]);
            setData([]);
        } finally {
            setLoading(false);
        }
    }, [tableName, loadData]);

    const handleTableChange = useCallback((pag: any, filters: any, sorter: any) => {
        const newPage = pag.current;
        const newSortField = sorter.field;
        const newSortOrder = sorter.order === 'descend' ? 'DESC' : 'ASC';

        if (newPage !== pagination.page) {
            setPagination(prev => ({ ...prev, page: newPage }));
            loadData(newPage, newSortField, newSortOrder);
        } else if (newSortField !== sortField || newSortOrder !== sortOrder) {
            setSortField(newSortField);
            setSortOrder(newSortOrder);
            loadData(1, newSortField, newSortOrder);
        }
    }, [pagination.page, sortField, sortOrder, loadData]);

    const handleDelete = useCallback((row: any) => {
        const pkCol = schema.find(c => c.pk)?.name || 'id';
        const pkVal = row[pkCol];

        if (!pkVal) {
            message.error('Cannot identify primary key for this row.');
            return;
        }

        confirm({
            title: 'Delete Row',
            icon: <ExclamationCircleOutlined />,
            content: `Are you sure you want to delete row where ${pkCol} = ${pkVal}?`,
            okText: 'Delete',
            okType: 'danger',
            cancelText: 'Cancel',
            onOk: async () => {
                try {
                    await DatabaseApiService.deleteRow(tableName, pkCol, pkVal);
                    message.success('Row deleted successfully');
                    loadData(pagination.page);
                } catch (err: any) {
                    message.error(`Delete failed: ${err.message}`);
                }
            },
        });
    }, [tableName, schema, pagination.page, loadData]);

    const startEdit = useCallback((row: any) => {
        setEditingRow(row);
        form.setFieldsValue(row);
    }, [form]);

    const handleSave = useCallback(async () => {
        try {
            const values = await form.validateFields();
            const pkCol = schema.find(c => c.pk)?.name || 'id';
            const pkVal = editingRow[pkCol];

            if (!pkVal) {
                message.error('Cannot identify primary key for update.');
                return;
            }

            const updates: any = {};
            let hasChanges = false;
            Object.keys(values).forEach(key => {
                if (values[key] !== editingRow[key]) {
                    updates[key] = values[key];
                    hasChanges = true;
                }
            });

            if (!hasChanges) {
                setEditingRow(null);
                return;
            }

            await DatabaseApiService.updateRow(tableName, pkCol, pkVal, updates);
            message.success('Row updated successfully');
            setEditingRow(null);
            loadData(pagination.page);
        } catch (err: any) {
            console.error('Validation failed:', err);
        }
    }, [tableName, editingRow, schema, form, pagination.page, loadData]);

    // --- Dynamic Column Generation ---
    const columns = React.useMemo(() => [
        {
            title: 'Actions',
            key: 'actions',
            fixed: 'left' as const,
            width: 80,
            render: (_: any, record: any) => (
                <Space size="small">
                    <Tooltip title="Edit">
                        <Button
                            type="text"
                            size="small"
                            icon={<EditOutlined className="text-blue-600" />}
                            onClick={(e) => { e.stopPropagation(); startEdit(record); }}
                        />
                    </Tooltip>
                    <Tooltip title="Delete">
                        <Button
                            type="text"
                            size="small"
                            danger
                            icon={<DeleteOutlined />}
                            onClick={(e) => { e.stopPropagation(); handleDelete(record); }}
                        />
                    </Tooltip>
                </Space>
            ),
        },
        ...schema.map(col => ({
            title: (
                <div style={{ whiteSpace: 'nowrap', minWidth: '120px' }}>
                    <Space size={4}>
                        {col.pk && <KeyOutlined className="text-yellow-500 text-[10px]" />}
                        <Text strong style={{ fontSize: '11px' }}>{col.name}</Text>
                        <Text type="secondary" style={{ fontSize: '9px', fontWeight: 400 }}>{col.type}</Text>
                    </Space>
                </div>
            ),
            dataIndex: col.name,
            key: col.name,
            sorter: true,
            width: 180,
            sortOrder: sortField === col.name ? (sortOrder === 'ASC' ? 'ascend' : 'descend') : null as any,
            render: (val: any) => {
                if (val === null || val === undefined) return <Text type="secondary" italic style={{ fontSize: '12px', opacity: 0.5 }}>null</Text>;
                if (typeof val === 'boolean') return <Tag color={val ? 'success' : 'error'}>{val.toString()}</Tag>;
                if (typeof val === 'object') return <Tooltip title={JSON.stringify(val)}><Text ellipsis style={{ maxWidth: 200, fontSize: '12px' }}>{JSON.stringify(val)}</Text></Tooltip>;
                return <Text style={{ fontSize: '12px', fontFamily: 'monospace' }} className="truncate block max-w-[300px]">{String(val)}</Text>;
            },
        }))
    ], [schema, sortField, sortOrder, startEdit, handleDelete]);

    return (
        <div className="flex flex-col h-full bg-white">
            {/* Toolbar */}
            <div className="px-6 py-4 border-b border-gray-100 flex justify-between items-center">
                <div>
                    <Space align="center" size="middle">
                        <Title level={5} style={{ margin: 0, fontWeight: 700 }}>{tableName}</Title>
                        <Space split={<div className="w-1 h-1 bg-gray-300 rounded-full" />}>
                            <Text type="secondary" style={{ fontSize: '12px' }}>{pagination.total} records</Text>
                            <Text type="secondary" style={{ fontSize: '12px' }}>{schema.length} columns</Text>
                            {schema.some(c => c.pk) && (
                                <Tag color="warning" icon={<KeyOutlined />} style={{ fontSize: '10px' }}>
                                    {schema.filter(c => c.pk).map(c => c.name).join(', ')}
                                </Tag>
                            )}
                        </Space>
                    </Space>
                </div>
                <Button
                    icon={<ReloadOutlined />}
                    onClick={() => loadSchemaAndData()}
                    loading={loading}
                >
                    Refresh
                </Button>
            </div>

            {/* Content Area */}
            <div className="flex-1 overflow-hidden p-4">
                {error && (
                    <div className="mb-4">
                        <Tag color="error" icon={<ExclamationCircleOutlined />} style={{ padding: '8px 16px', display: 'flex', alignItems: 'center', width: '100%' }}>
                            {error}
                        </Tag>
                    </div>
                )}

                <Table
                    columns={columns}
                    dataSource={data}
                    loading={loading}
                    rowKey={record => record.id || record._row_idx}
                    pagination={{
                        current: pagination.page,
                        pageSize: pagination.limit,
                        total: pagination.total,
                        showSizeChanger: false,
                        showTotal: (total) => `Total ${total} items`,
                    }}
                    onChange={handleTableChange}
                    onRow={(record) => ({
                        onClick: () => {
                            const pkCol = schema.find(c => c.pk)?.name || 'id';
                            const pkVal = record[pkCol] || record._row_idx;
                            setSelectedRowId(pkVal);
                        }
                    })}
                    rowClassName={(record) => {
                        const pkCol = schema.find(c => c.pk)?.name || 'id';
                        const pkVal = record[pkCol] || record._row_idx;
                        return pkVal === selectedRowId ? 'bg-blue-50/80 highlighted-row' : '';
                    }}
                    scroll={{ x: 'max-content', y: 'calc(100vh - 350px)' }}
                    size="small"
                    className="mgmt-table"
                    bordered
                />
            </div>


            {/* Edit Modal */}
            <Modal
                title={
                    <Space>
                        <EditOutlined className="text-blue-600" />
                        <span>Edit Record</span>
                    </Space>
                }
                open={!!editingRow}
                onOk={handleSave}
                onCancel={() => setEditingRow(null)}
                width={600}
                destroyOnClose
                footer={[
                    <Button key="cancel" onClick={() => setEditingRow(null)} icon={<CloseOutlined />}>
                        Cancel
                    </Button>,
                    <Button key="save" type="primary" onClick={handleSave} icon={<SaveOutlined />}>
                        Save Changes
                    </Button>
                ]}
            >
                <div style={{ maxHeight: '60vh', overflowY: 'auto', paddingRight: '8px' }}>
                    <Form
                        form={form}
                        layout="vertical"
                        requiredMark="optional"
                    >
                        {schema.map(col => (
                            <Form.Item
                                key={col.name}
                                name={col.name}
                                label={
                                    <Space size={4}>
                                        <Text strong style={{ fontSize: '13px' }}>{col.name}</Text>
                                        <Text type="secondary" style={{ fontSize: '11px', fontWeight: 400 }}>({col.type})</Text>
                                        {col.pk && <Tag color="warning" style={{ fontSize: '10px', height: '18px', lineHeight: '16px' }}>PK</Tag>}
                                    </Space>
                                }
                                rules={[{ required: col.notNull && !col.pk, message: `${col.name} is required` }]}
                            >
                                {col.pk ? (
                                    <Input disabled style={{ background: '#f8fafc', color: '#64748b' }} />
                                ) : col.type.includes('INT') || col.type.includes('NUM') ? (
                                    <InputNumber style={{ width: '100%' }} />
                                ) : col.type.toLowerCase().includes('bool') ? (
                                    <Select options={[
                                        { label: 'True', value: true },
                                        { label: 'False', value: false }
                                    ]} />
                                ) : (
                                    <Input.TextArea autoSize={{ minRows: 1, maxRows: 4 }} />
                                )}
                            </Form.Item>
                        ))}
                    </Form>
                </div>
            </Modal>

            <style>{`
                .mgmt-table .ant-table-thead > tr > th {
                    background: #f8fafc;
                    font-weight: 700;
                }
                .mgmt-table .ant-table-body {
                    scrollbar-width: thin;
                }
            `}</style>
        </div>
    );
};

export default DataTable;

