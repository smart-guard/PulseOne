
import React from 'react';
import { render, screen, fireEvent, waitFor } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { vi, describe, it, expect, beforeEach } from 'vitest';
import GroupSidePanel from '../GroupSidePanel';
import { GroupApiService } from '../../../../api/services/groupApi';
import { ConfirmProvider } from '../../../../components/common/ConfirmProvider';
import * as matchers from '@testing-library/jest-dom/matchers';
expect.extend(matchers);

// Mock the API Service
vi.mock('../../../../api/services/groupApi', () => ({
    GroupApiService: {
        getGroupTree: vi.fn(),
        createGroup: vi.fn(),
        updateGroup: vi.fn(),
        deleteGroup: vi.fn(),
    }
}));

describe('GroupSidePanel Integration Test', () => {
    const mockOnGroupSelect = vi.fn();

    beforeEach(() => {
        vi.clearAllMocks();
    });

    it('should allow user to create a top-level group and display it', async () => {
        const user = userEvent.setup();

        // 1. Initial State: Empty Tree
        (GroupApiService.getGroupTree as any).mockResolvedValueOnce({
            success: true,
            data: []
        });

        render(
            <ConfirmProvider>
                <GroupSidePanel selectedGroupId="all" onGroupSelect={mockOnGroupSelect} />
            </ConfirmProvider>
        );

        // Verify initial load
        await waitFor(() => expect(GroupApiService.getGroupTree).toHaveBeenCalledTimes(1));

        // 2. Click "Group Management" to enter manage mode
        const manageBtn = screen.getByText('그룹 관리');
        await user.click(manageBtn);

        // 3. Click "Top Level Group"
        const createRootBtn = screen.getByText('최상위 그룹');
        await user.click(createRootBtn);

        // 4. Fill in the form
        const input = screen.getByPlaceholderText('그룹 이름');
        await user.type(input, 'Frontend Test A');

        // 5. Setup Mock for Creation Success AND Re-fetch
        (GroupApiService.createGroup as any).mockResolvedValueOnce({
            success: true,
            data: { id: 1, name: 'Frontend Test A', tenant_id: 1, parent_group_id: null }
        });

        (GroupApiService.getGroupTree as any).mockResolvedValueOnce({
            success: true,
            data: [{ id: 1, name: 'Frontend Test A', tenant_id: 1, parent_group_id: null, children: [] }]
        });

        // 6. Submit
        // Since it's inside a form, pressing Enter on input works, or firing submit on form
        fireEvent.submit(input.closest('form')!);

        // 7. Verification
        await waitFor(() => {
            expect(GroupApiService.createGroup).toHaveBeenCalledWith(
                expect.objectContaining({ name: 'Frontend Test A', parent_group_id: null })
            );
        });

        // Verify re-fetch
        await waitFor(() => {
            // Should have been called twice (init + after create)
            expect(GroupApiService.getGroupTree).toHaveBeenCalledTimes(2);
        });

        // Verify Display
        // "Frontend Test A" should now be visible in the list
        expect(await screen.findByText('Frontend Test A')).toBeInTheDocument();
    });
});
